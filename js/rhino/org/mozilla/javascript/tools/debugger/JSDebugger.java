/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino JavaScript Debugger code, released
 * November 21, 2000.
 *
 * The Initial Developer of the Original Code is See Beyond Corporation.

 * Portions created by See Beyond are
 * Copyright (C) 2000 See Beyond Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Christopher Oliver
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.tools.debugger;

import javax.swing.*;
import javax.swing.text.*;
import javax.swing.event.*;
import javax.swing.table.*;
import java.awt.*;
import java.awt.event.*;
import java.util.StringTokenizer;

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.*;
import org.mozilla.javascript.tools.shell.Main;
import org.mozilla.javascript.tools.shell.ConsoleTextArea;
import java.util.*;
import java.io.*;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.DefaultTreeCellRenderer;
import java.lang.reflect.Method;

class MessageDialogWrapper {
    static void showMessageDialog(Component parent, String msg, String title,
				  int flags) {
	JOptionPane.showMessageDialog(parent,
				      msg,
				      title,
				      flags);
    }
};

class EvalTextArea extends JTextArea implements KeyListener,
DocumentListener {
    JSDebugger db;
    private java.util.Vector history;
    private int historyIndex = -1;
    private int outputMark = 0;

    public void select(int start, int end) {
	requestFocus();
	super.select(start, end);
    }

    public EvalTextArea(JSDebugger db) {
        super();
	this.db = db;
        history = new java.util.Vector();
        Document doc = getDocument();
	doc.addDocumentListener(this);
        addKeyListener(this);
        setLineWrap(true);
        setFont(new Font("Monospaced", 0, 12));
	append("% ");
	outputMark = doc.getLength();
    }

    synchronized void returnPressed() {
        Document doc = getDocument();
        int len = doc.getLength();
        Segment segment = new Segment();
        try {
            doc.getText(outputMark, len - outputMark, segment);
        } catch(javax.swing.text.BadLocationException ignored) {
            ignored.printStackTrace();
        }
	String text = segment.toString();
	Context cx = Context.getCurrentContext();
	if(cx.stringIsCompilableUnit(text)) {
	    history.addElement(text);
	    historyIndex = history.size();
	    String result = db.eval(text);
	    append("\n");
	    append(result);
	    append("\n% ");
	    outputMark = doc.getLength();
	} else {
	    append("\n");
	}
    }
    
    public void keyPressed(KeyEvent e) {
        int code = e.getKeyCode();
        if(code == KeyEvent.VK_BACK_SPACE || code == KeyEvent.VK_LEFT) {
            if(outputMark == getCaretPosition()) {
                e.consume();
            }
        } else if(code == KeyEvent.VK_HOME) {
           int caretPos = getCaretPosition();
           if(caretPos == outputMark) {
               e.consume();
           } else if(caretPos > outputMark) {
               if(!e.isControlDown()) {
                   if(e.isShiftDown()) {
                       moveCaretPosition(outputMark);
                   } else {
                       setCaretPosition(outputMark);
                   }
                   e.consume();
               }
           }
        } else if(code == KeyEvent.VK_ENTER) {
            returnPressed();
            e.consume();
        } else if(code == KeyEvent.VK_UP) {
            historyIndex--;
            if(historyIndex >= 0) {
                if(historyIndex >= history.size()) {
                    historyIndex = history.size() -1;
                }
                if(historyIndex >= 0) {
                    String str = (String)history.elementAt(historyIndex);
                    int len = getDocument().getLength();
                    replaceRange(str, outputMark, len);
                    int caretPos = outputMark + str.length();
                    select(caretPos, caretPos);
                } else {
                    historyIndex++;
                }
            } else {
                historyIndex++;
            }
            e.consume();
        } else if(code == KeyEvent.VK_DOWN) {
            int caretPos = outputMark;
            if(history.size() > 0) {
                historyIndex++;
                if(historyIndex < 0) {historyIndex = 0;}
                int len = getDocument().getLength();
                if(historyIndex < history.size()) {
                    String str = (String)history.elementAt(historyIndex);
                    replaceRange(str, outputMark, len);
                    caretPos = outputMark + str.length();
                } else {
                    historyIndex = history.size();
                    replaceRange("", outputMark, len);
                }
            }
            select(caretPos, caretPos);
            e.consume();
        }
    }
 
    public void keyTyped(KeyEvent e) {
        int keyChar = e.getKeyChar();
        if(keyChar == 0x8 /* KeyEvent.VK_BACK_SPACE */) {
            if(outputMark == getCaretPosition()) {
                e.consume();
            }
        } else if(getCaretPosition() < outputMark) {
            setCaretPosition(outputMark);
        }
    }
    
    public synchronized void keyReleased(KeyEvent e) {
    }

    public synchronized void write(String str) {
        insert(str, outputMark);
        int len = str.length();
        outputMark += len;
        select(outputMark, outputMark);
    }
    
    public synchronized void insertUpdate(DocumentEvent e) {
        int len = e.getLength();
        int off = e.getOffset();
        if(outputMark > off) {
            outputMark += len;
        }
    }
    
    public synchronized void removeUpdate(DocumentEvent e) {
        int len = e.getLength();
        int off = e.getOffset();
        if(outputMark > off) {
            if(outputMark >= off + len) {
                outputMark -= len;
            } else {
                outputMark = off;
            }
        }
    }

    public synchronized void postUpdateUI() {
        // this attempts to cleanup the damage done by updateComponentTreeUI
        requestFocus();
        setCaret(getCaret());
        select(outputMark, outputMark);
    }
    
    public synchronized void changedUpdate(DocumentEvent e) {
    }
};

class EvalWindow extends JInternalFrame 
implements ActionListener {
    
    EvalTextArea evalTextArea;

    public void setEnabled(boolean b) {
	super.setEnabled(b);
	evalTextArea.setEnabled(b);
    }

    public EvalWindow(String name, JSDebugger db) {
        super(name, true, false, true, true);
        evalTextArea = new EvalTextArea(db);
	evalTextArea.setRows(24);
	evalTextArea.setColumns(80);
        JScrollPane scroller = new JScrollPane(evalTextArea);
        setContentPane(scroller);
        //scroller.setPreferredSize(new Dimension(600, 400));
        pack();
        setVisible(true);
    }

    public void actionPerformed(ActionEvent e) {
        String cmd = e.getActionCommand();
        if(cmd.equals("Cut")) {
            evalTextArea.cut();
        } else if(cmd.equals("Copy")) {
            evalTextArea.copy();
        } else if(cmd.equals("Paste")) {
            evalTextArea.paste();
        } 
    }
};

class JSInternalConsole extends JInternalFrame 
implements ActionListener {
    
    ConsoleTextArea consoleTextArea;

    public InputStream getIn() {
	return consoleTextArea.getIn();
    }

    public PrintStream getOut() {
	return consoleTextArea.getOut();
    }

    public PrintStream getErr() {
	return consoleTextArea.getErr();
    }

    public JSInternalConsole(String name) {
        super(name, true, false, true, true);
        consoleTextArea = new ConsoleTextArea(null);
	consoleTextArea.setRows(24);
	consoleTextArea.setColumns(80);
        JScrollPane scroller = new JScrollPane(consoleTextArea);
        setContentPane(scroller);
        pack();
        setVisible(true);
    }

    public void actionPerformed(ActionEvent e) {
        String cmd = e.getActionCommand();
        if(cmd.equals("Cut")) {
            consoleTextArea.cut();
        } else if(cmd.equals("Copy")) {
            consoleTextArea.copy();
        } else if(cmd.equals("Paste")) {
            consoleTextArea.paste();
        } 
    }
};

class FilePopupMenu extends JPopupMenu {
    FileTextArea w;
    int x, y;

    FilePopupMenu(FileTextArea w) {
	super();
	this.w = w;
	JMenuItem item;
	add(item = new JMenuItem("Set Breakpoint"));
	item.addActionListener(w);
	add(item = new JMenuItem("Clear Breakpoint"));
	item.addActionListener(w);
	add(item = new JMenuItem("Run to Cursor"));
	item.addActionListener(w);
    }
    void show(JComponent comp, int x, int y) {
	this.x = x;
	this.y = y;
	super.show(comp, x, y);
    }
};

class FileTextArea extends JTextArea implements ActionListener,
						PopupMenuListener,
						KeyListener,
						MouseListener {
    FileWindow w;
    FilePopupMenu popup;

    FileTextArea(FileWindow w) {
	this.w = w;
	popup = new FilePopupMenu(this);
	popup.addPopupMenuListener(this);
	addMouseListener(this);
	addKeyListener(this);
	setFont(new Font("Monospaced", 0, 12));
    }

    void select(int pos) {
	if(pos >= 0) {
	    try {
		int line = getLineOfOffset(pos);
		Rectangle rect = modelToView(pos);
		if(rect == null) {
		    select(pos, pos);
		} else {
		    try {
			Rectangle nrect = 
			    modelToView(getLineStartOffset(line + 1));
			if(nrect != null) {
			    rect = nrect;
			}
		    } catch(Exception exc) {
		    }
		    JViewport vp = (JViewport)getParent();
		    Rectangle viewRect = vp.getViewRect();
		    if(viewRect.y + viewRect.height > rect.y) {
			// need to scroll up
			select(pos, pos);
		    } else {
			// need to scroll down
			rect.y += (viewRect.height - rect.height)/2;
			scrollRectToVisible(rect);
			select(pos, pos);
		    }
		}
	    } catch(BadLocationException exc) {
		select(pos, pos);
		//exc.printStackTrace();
	    }
	}
    }


    public void mousePressed(MouseEvent e) {
	checkPopup(e);
    }
    public void mouseClicked(MouseEvent e) {
	checkPopup(e);
    }
    public void mouseEntered(MouseEvent e) {
    }
    public void mouseExited(MouseEvent e) {
    }
    public void mouseReleased(MouseEvent e) {
	checkPopup(e);
    }

    private void checkPopup(MouseEvent e) {
	if(e.isPopupTrigger()) {
	    popup.show(this, e.getX(), e.getY());
	}
    }

    public void popupMenuWillBecomeVisible(PopupMenuEvent e) {
    }
    public void popupMenuWillBecomeInvisible(PopupMenuEvent e) {
    }
    public void popupMenuCanceled(PopupMenuEvent e) {
    }
    public void actionPerformed(ActionEvent e) {
	int pos = viewToModel(new Point(popup.x, popup.y));
	popup.setVisible(false);
	String cmd = e.getActionCommand();
	int line = -1;
	try {
	    line = getLineOfOffset(pos);
	} catch(Exception exc) {
	}
	if(cmd.equals("Set Breakpoint")) {
	    w.setBreakPoint(line + 1);
	} else if(cmd.equals("Clear Breakpoint")) {
	    w.clearBreakPoint(line + 1);
	} else if(cmd.equals("Run to Cursor")) {
	    w.runToCursor(e);
	}
    }
    public void keyPressed(KeyEvent e) {
	switch(e.getKeyCode()) {
	case KeyEvent.VK_BACK_SPACE:
	case KeyEvent.VK_ENTER:
	case KeyEvent.VK_DELETE:
	    e.consume();
	    break;
	}
    }
    public void keyTyped(KeyEvent e) {
	e.consume();
    }
    public void keyReleased(KeyEvent e) {
	e.consume();
    }
}

class MoreWindows extends JDialog implements ActionListener {
  private String value = null;
  private JList list;
  Hashtable fileWindows;
  JButton setButton;
  JButton refreshButton;
  JButton cancelButton;

    /**
     * Show the initialized dialog.  The first argument should
     * be null if you want the dialog to come up in the center
     * of the screen.  Otherwise, the argument should be the
     * component on top of which the dialog should appear.
     */

    public String showDialog(Component comp) {
	value = null;
	setLocationRelativeTo(comp);
	setVisible(true);
	return value;
    }

    private void setValue(String newValue) {
	value = newValue;
	list.setSelectedValue(value, true);
    }

    public void actionPerformed(ActionEvent e) {
	String cmd = e.getActionCommand();
	if(cmd.equals("Cancel")) {
	    setVisible(false);
	    value = null;
	} else if(cmd.equals("Select")) {
	    value = (String)list.getSelectedValue();
	    setVisible(false);
	    JInternalFrame w = (JInternalFrame)fileWindows.get(value);
	    if(w != null) {
		try {
		    w.show();
		    w.setSelected(true);
		} catch(Exception exc) {
		}
	    }
	} 
    }
    class MouseHandler extends MouseAdapter {
	public void mouseClicked(MouseEvent e) {
	    if (e.getClickCount() == 2) {
		setButton.doClick();
	    }
	}
  };

    MoreWindows(JFrame frame, Hashtable fileWindows,
		String title,
		String labelText) {
    super(frame, title, true);
    this.fileWindows = fileWindows;
    //buttons
    cancelButton = new JButton("Cancel");
    setButton = new JButton("Select");
    cancelButton.addActionListener(this);
    setButton.addActionListener(this);
    getRootPane().setDefaultButton(setButton);
    
    //main part of the dialog
    list = new JList(new DefaultListModel());
    DefaultListModel model = (DefaultListModel)list.getModel();
    model.clear();
    //model.fireIntervalRemoved(model, 0, size);
    Enumeration e = fileWindows.keys();
    while(e.hasMoreElements()) {
	String data = e.nextElement().toString();
	model.addElement(data);
    }
    list.setSelectedIndex(0);
    //model.fireIntervalAdded(model, 0, data.length);
    setButton.setEnabled(true);
    list.setSelectionMode(ListSelectionModel.SINGLE_INTERVAL_SELECTION);
    list.addMouseListener(new MouseHandler());
    JScrollPane listScroller = new JScrollPane(list);
    listScroller.setPreferredSize(new Dimension(320, 240));
    //XXX: Must do the following, too, or else the scroller thinks
    //XXX: it's taller than it is:
    listScroller.setMinimumSize(new Dimension(250, 80));
    listScroller.setAlignmentX(LEFT_ALIGNMENT);
    
    //Create a container so that we can add a title around
    //the scroll pane.  Can't add a title directly to the 
    //scroll pane because its background would be white.
    //Lay out the label and scroll pane from top to button.
    JPanel listPane = new JPanel();
    listPane.setLayout(new BoxLayout(listPane, BoxLayout.Y_AXIS));
    JLabel label = new JLabel(labelText);
    label.setLabelFor(list);
    listPane.add(label);
    listPane.add(Box.createRigidArea(new Dimension(0,5)));
    listPane.add(listScroller);
    listPane.setBorder(BorderFactory.createEmptyBorder(10,10,10,10));
    
    //Lay out the buttons from left to right.
    JPanel buttonPane = new JPanel();
    buttonPane.setLayout(new BoxLayout(buttonPane, BoxLayout.X_AXIS));
    buttonPane.setBorder(BorderFactory.createEmptyBorder(0, 10, 10, 10));
    buttonPane.add(Box.createHorizontalGlue());
    buttonPane.add(cancelButton);
    buttonPane.add(Box.createRigidArea(new Dimension(10, 0)));
    buttonPane.add(setButton);
    
    //Put everything together, using the content pane's BorderLayout.
    Container contentPane = getContentPane();
    contentPane.add(listPane, BorderLayout.CENTER);
    contentPane.add(buttonPane, BorderLayout.SOUTH);
    pack();
  }
};

class FindFunction extends JDialog implements ActionListener {
  private String value = null;
  private JList list;
    Hashtable functionMap;
  JSDebugger db;
  JButton setButton;
  JButton refreshButton;
  JButton cancelButton;

    /**
     * Show the initialized dialog.  The first argument should
     * be null if you want the dialog to come up in the center
     * of the screen.  Otherwise, the argument should be the
     * component on top of which the dialog should appear.
     */

    public String showDialog(Component comp) {
	value = null;
	setLocationRelativeTo(comp);
	setVisible(true);
	return value;
    }

    private void setValue(String newValue) {
	value = newValue;
	list.setSelectedValue(value, true);
    }

    public void actionPerformed(ActionEvent e) {
	String cmd = e.getActionCommand();
	if(cmd.equals("Cancel")) {
	    setVisible(false);
	    value = null;
	} else if(cmd.equals("Select")) {
	    if(list.getSelectedIndex() < 0) {
		return;
	    }
	    try {
		value = (String)list.getSelectedValue();
	    } catch(ArrayIndexOutOfBoundsException exc) {
		return;
	    }
	    setVisible(false);
	    JSDebugger.SourceEntry sourceEntry = (JSDebugger.SourceEntry)functionMap.get(value);
	    DebuggableScript script = sourceEntry.fnOrScript;
	    if(script != null) {
		String sourceName = script.getSourceName();
		try {
		    sourceName = new File(sourceName).getCanonicalPath();
		} catch(IOException exc) {
		}
		Enumeration ee = script.getLineNumbers();
		int lineNumber = ((Integer)ee.nextElement()).intValue();
		FileWindow w = db.getFileWindow(sourceName);
		if(w == null) {
		    (new CreateFileWindow(db, sourceName, sourceEntry.source.toString(), lineNumber)).run();
		    w = db.getFileWindow(sourceName);
		}
		int start = w.getPosition(lineNumber-1);
		w.select(start, start);
		try {
		    w.show();
		    w.setSelected(true);
		} catch(Exception exc) {
		}
	    }
	} 
    }

    class MouseHandler extends MouseAdapter {
	public void mouseClicked(MouseEvent e) {
	    if (e.getClickCount() == 2) {
		setButton.doClick();
	    }
	}
    };

    FindFunction(JSDebugger db, Hashtable functionMap,
		 String title,
		 String labelText) {
	super(db, title, true);
	this.functionMap = functionMap;
	this.db = db;
	//buttons
	cancelButton = new JButton("Cancel");
	setButton = new JButton("Select");
	cancelButton.addActionListener(this);
	setButton.addActionListener(this);
	getRootPane().setDefaultButton(setButton);
	
	//main part of the dialog
	list = new JList(new DefaultListModel());
	DefaultListModel model = (DefaultListModel)list.getModel();
	model.clear();
	//model.fireIntervalRemoved(model, 0, size);
	Enumeration e = functionMap.keys();
	String[] a = new String[functionMap.size()];
	int i = 0;
	while(e.hasMoreElements()) {
	    a[i++] = e.nextElement().toString();
	}
	java.util.Arrays.sort(a);
	for(i = 0; i < a.length; i++) {
	    model.addElement(a[i]);
	}
	list.setSelectedIndex(0);
	//model.fireIntervalAdded(model, 0, data.length);
	setButton.setEnabled(a.length > 0);
	list.setSelectionMode(ListSelectionModel.SINGLE_INTERVAL_SELECTION);
	list.addMouseListener(new MouseHandler());
	JScrollPane listScroller = new JScrollPane(list);
	listScroller.setPreferredSize(new Dimension(320, 240));
	//XXX: Must do the following, too, or else the scroller thinks
	//XXX: it's taller than it is:
	listScroller.setMinimumSize(new Dimension(250, 80));
	listScroller.setAlignmentX(LEFT_ALIGNMENT);
	
	//Create a container so that we can add a title around
	//the scroll pane.  Can't add a title directly to the 
	//scroll pane because its background would be white.
	//Lay out the label and scroll pane from top to button.
	JPanel listPane = new JPanel();
	listPane.setLayout(new BoxLayout(listPane, BoxLayout.Y_AXIS));
	JLabel label = new JLabel(labelText);
	label.setLabelFor(list);
	listPane.add(label);
	listPane.add(Box.createRigidArea(new Dimension(0,5)));
	listPane.add(listScroller);
	listPane.setBorder(BorderFactory.createEmptyBorder(10,10,10,10));
	
	//Lay out the buttons from left to right.
	JPanel buttonPane = new JPanel();
	buttonPane.setLayout(new BoxLayout(buttonPane, BoxLayout.X_AXIS));
	buttonPane.setBorder(BorderFactory.createEmptyBorder(0, 10, 10, 10));
	buttonPane.add(Box.createHorizontalGlue());
	buttonPane.add(cancelButton);
	buttonPane.add(Box.createRigidArea(new Dimension(10, 0)));
	buttonPane.add(setButton);
	
	//Put everything together, using the content pane's BorderLayout.
	Container contentPane = getContentPane();
	contentPane.add(listPane, BorderLayout.CENTER);
	contentPane.add(buttonPane, BorderLayout.SOUTH);
	pack();
    }
};

class FileHeader extends JPanel implements MouseListener {

    FileWindow fileWindow;

    public void mouseEntered(MouseEvent e) {
    }
    public void mousePressed(MouseEvent e) {
	if(e.getComponent() == this && 
	  (e.getModifiers() & MouseEvent.BUTTON1_MASK) != 0) {
	    int x = e.getX();
	    int y = e.getY();
	    Font font = fileWindow.textArea.getFont();
	    FontMetrics metrics = getFontMetrics(font);
	    int h = metrics.getHeight();
	    int line = y/h;
	    fileWindow.toggleBreakPoint(line + 1);
	}
    }
    public void mouseClicked(MouseEvent e) {
    }
    public void mouseExited(MouseEvent e) {
    }
    public void mouseReleased(MouseEvent e) {
    }

    FileHeader(FileWindow fileWindow) {
	this.fileWindow = fileWindow;
	addMouseListener(this);
	update();
    }

    void update() {
	FileTextArea textArea = fileWindow.textArea;
	Font font = textArea.getFont();
        setFont(font);
        FontMetrics metrics = getFontMetrics(font);
	int h = metrics.getHeight();
	int lineCount = textArea.getLineCount() + 1;
	String dummy = Integer.toString(lineCount);
	if(dummy.length() < 2) {
	    dummy = "99";
	}
	Dimension d = new Dimension();
	d.width = metrics.stringWidth(dummy) + 16;
	d.height = lineCount * h + 100;
	setPreferredSize(d);
	setSize(d);
    }

    public void paint(Graphics g) {
	super.paint(g);
	FileTextArea textArea = fileWindow.textArea;
	Font font = textArea.getFont();
        g.setFont(font);
        FontMetrics metrics = getFontMetrics(font);
        Rectangle clip = g.getClipBounds();
	g.setColor(getBackground());
        g.fillRect(clip.x, clip.y, clip.width, clip.height);
        int left = getX();
        int ascent = metrics.getMaxAscent();
	int h = metrics.getHeight();
	int lineCount = textArea.getLineCount() + 1;
	String dummy = Integer.toString(lineCount);
	if(dummy.length() < 2) {
	    dummy = "99";
	}
	int maxWidth = metrics.stringWidth(dummy);
        int startLine = clip.y / h;
        int endLine = (clip.y + clip.height) / h + 1;
	int width = getWidth();
	if(endLine > lineCount) endLine = lineCount;
	for(int i = startLine; i < endLine; i++) {
            String text;
	    int pos = -2; 
	    try {
		pos = textArea.getLineStartOffset(i);
	    } catch(BadLocationException ignored) {
	    }
	    boolean isBreakPoint = false;
	    if(fileWindow.breakpoints.get(new Integer(pos)) != null) {
		isBreakPoint = true;
	    }
	    text = Integer.toString(i + 1) + " ";
	    int w = metrics.stringWidth(text);
	    int y = i *h;
	    g.setColor(Color.blue);
	    g.drawString(text, 0, y + ascent);
	    int x = width - ascent;
	    if(isBreakPoint) {
		g.setColor(new Color(0x80, 0x00, 0x00));
		int dy = y + ascent - 9;
		g.fillOval(x, dy, 9, 9);
		g.drawOval(x, dy, 8, 8);
		g.drawOval(x, dy, 9, 9);
	    }
	    if(pos == fileWindow.currentPos) {
		Polygon arrow = new Polygon();
		int dx = x;
		y += ascent - 10;
		int dy = y;
		arrow.addPoint(dx, dy + 3);
		arrow.addPoint(dx + 5, dy + 3);
		for(x = dx + 5; x <= dx + 10; x++, y++) {
		    arrow.addPoint(x, y);
		}
		for(x = dx + 9; x >= dx + 5; x--, y++) {
		    arrow.addPoint(x, y);
		}
		arrow.addPoint(dx + 5, dy + 7);
		arrow.addPoint(dx, dy + 7);
		g.setColor(Color.yellow);
		g.fillPolygon(arrow);
		g.setColor(Color.black);
		g.drawPolygon(arrow);
	    } 
        }
    }
};

class FileWindow extends JInternalFrame {

    JSDebugger db;
    FileTextArea textArea;
    FileHeader fileHeader;
    JScrollPane p;
    int currentPos;
    Hashtable breakpoints;
    String url;
    JLabel statusBar;
    int lineNumChars;
    String cursor = ">";
    String breakPoint = "*";

    public void dispose() {
	Enumeration e = breakpoints.keys();
	while(e.hasMoreElements()) {
	    Integer line = (Integer)breakpoints.get(e.nextElement());
	    db.clearBreakPoint(url, line.intValue());
	}
	db.removeWindow(this);
	super.dispose();
    }

    void runToCursor(ActionEvent e) {
	try {
	    db.runToCursor(url,
			   textArea.getLineOfOffset(textArea.getCaretPosition()) + 1,
			   e);
	} catch(BadLocationException exc) {
	}
    }

   public int getPosition(int line) {
	int result = -1;
	try {
	    result = textArea.getLineStartOffset(line);
	} catch(javax.swing.text.BadLocationException exc) {
	}
	return result;
    }

    boolean isBreakPoint(int line) {
	int pos = getPosition(line - 1);
	return breakpoints.get(new Integer(pos)) != null;
    }

    void toggleBreakPoint(int line) {
	int pos = getPosition(line - 1);
	Integer i = new Integer(pos);
	if(breakpoints.get(i) == null) {
	    setBreakPoint(line);
	} else {
	    clearBreakPoint(line);
	}
    }
    
    void setBreakPoint(int line) {
	int actualLine = db.setBreakPoint(url, line);
	if(actualLine != -1) {
	    int pos = getPosition(actualLine - 1);
	    breakpoints.put(new Integer(pos), new Integer(line));
	    fileHeader.repaint();
	}
    }

    void clearBreakPoint(int line) {
	db.clearBreakPoint(url, line);
	int pos = getPosition(line - 1);
	Integer loc = new Integer(pos);
	if(breakpoints.get(loc) != null) {
	    breakpoints.remove(loc);
	    fileHeader.repaint();
	}
    }

    FileWindow(JSDebugger db, String fileName, String text) {
	super(new File(fileName).getName(), true, true, true, true);
	this.db = db;
	breakpoints = (Hashtable)db.breakpointsMap.get(fileName);
	if(breakpoints == null) {
	    breakpoints = new Hashtable();
	    db.breakpointsMap.put(fileName, breakpoints);
	}
	setUrl(fileName);
	currentPos = -1;
	textArea = new FileTextArea(this);
	textArea.setRows(24);
	textArea.setColumns(80);
	p = new JScrollPane();
	fileHeader = new FileHeader(this);
	p.setViewportView(textArea);
	p.setRowHeaderView(fileHeader);
	setContentPane(p);
	setText(text);
	textArea.select(0);
    }

    public void setUrl(String fileName) {
	// in case fileName is very long, try to set tool tip on frame 
	Component c = getComponent(1);
	// this will work at least for Metal L&F
	if(c != null && c instanceof JComponent) {
	    ((JComponent)c).setToolTipText(fileName);
	}
	url = fileName;
    }

    public String getUrl() {
	return url;
    }

    void setText(String newText) {
	if(!textArea.getText().equals(newText)) {
	    textArea.setText(newText);
	    int pos = 0;
	    if(currentPos != -1) {
		pos = currentPos;
	    }
	    textArea.select(pos);
	}
	Enumeration e = breakpoints.keys();
	while(e.hasMoreElements()) {
	    Object key = e.nextElement();
	    Integer line = (Integer)breakpoints.get(key);
	    if(db.setBreakPoint(url, line.intValue()) == -1) {
		breakpoints.remove(key);
	    }
	}
	fileHeader.update();
	fileHeader.repaint();
    }

    void setPosition(int pos) {
	textArea.select(pos);
	currentPos = pos;
	fileHeader.repaint();
    }

    void select(int start, int end) {
	int docEnd = textArea.getDocument().getLength();
	textArea.select(docEnd, docEnd);
	textArea.select(start, end);
    }
};

class MyTableModel extends AbstractTableModel {
    JSDebugger db;
    Vector expressions;
    Vector values;
    MyTableModel(JSDebugger db) {
	this.db = db;
	expressions = new Vector();
	values = new Vector();
	expressions.addElement("");
	values.addElement("");
    }

    public int getColumnCount() {
	return 2;
    }

    public int getRowCount() {
	return expressions.size();
    }

    public String getColumnName(int column) {
	switch(column) {
	case 0:
	    return "Expression";
	case 1:
	    return "Value";
	}
	return null;
    }

    public boolean isCellEditable(int row, int column) {
	return true;
    }

    public Object getValueAt(int row, int column) {
	switch(column) {
	case 0:
	    return expressions.elementAt(row);
	case 1:
	    return values.elementAt(row);
	}
	return "";
    }

    public void setValueAt(Object value, int row, int column) {
	switch(column) {
	case 0:
	    String expr = value.toString();
	    expressions.setElementAt(expr, row);
	    String result = "";
	    if(expr.length() > 0) {
		result = db.eval(expr);
		if(result == null) result = "";
	    }
	    values.setElementAt(result, row);
	    updateModel();
	    if(row + 1 == expressions.size()) {
		expressions.addElement("");
		values.addElement("");
		fireTableRowsInserted(row + 1, row + 1);
	    }
	    break;
	case 1:
	    // just reset column 2; ignore edits
	    fireTableDataChanged();
	}
    }

    void updateModel() {
	for(int i = 0; i < expressions.size(); ++i) {
	    Object value = expressions.elementAt(i);
	    String expr = value.toString();
	    String result = "";
	    if(expr.length() > 0) {
		result = db.eval(expr);
		if(result == null) result = "";
	    } else {
		result = "";
	    }
	    result = result.replace('\n', ' ');
	    values.setElementAt(result, i);
	}
	fireTableDataChanged();
    }
};

class Evaluator extends JTable {
    MyTableModel tableModel;
    Evaluator(JSDebugger db) {
	super(new MyTableModel(db));
	tableModel = (MyTableModel)getModel();
    }
}


class MyTreeTable extends JTreeTable {

  public MyTreeTable(TreeTableModel model) {
    super(model);
  }

  public JTree resetTree(TreeTableModel treeTableModel) {
    tree = new TreeTableCellRenderer(treeTableModel); 
    
    // Install a tableModel representing the visible rows in the tree. 
    super.setModel(new TreeTableModelAdapter(treeTableModel, tree));
    
    // Force the JTable and JTree to share their row selection models. 
    ListToTreeSelectionModelWrapper selectionWrapper = new 
      ListToTreeSelectionModelWrapper();
    tree.setSelectionModel(selectionWrapper);
    setSelectionModel(selectionWrapper.getListSelectionModel()); 
    
    // Make the tree and table row heights the same. 
    if (tree.getRowHeight() < 1) {
      // Metal looks better like this.
      setRowHeight(18);
    }
    
    // Install the tree editor renderer and editor. 
    setDefaultRenderer(TreeTableModel.class, tree); 
    setDefaultEditor(TreeTableModel.class, new TreeTableCellEditor());  
    setShowGrid(true);
    setIntercellSpacing(new Dimension(1,1)); 	        
    tree.setRootVisible(false);
    tree.setShowsRootHandles(true);
    DefaultTreeCellRenderer r = (DefaultTreeCellRenderer)tree.getCellRenderer();
    r.setOpenIcon(null);
    r.setClosedIcon(null);
    r.setLeafIcon(null);
    return tree;
  }
};

class ContextWindow extends JPanel implements ActionListener {
    JComboBox context;
    Vector toolTips;
    JTabbedPane tabs;
    JTabbedPane tabs2;
    MyTreeTable thisTable;
    MyTreeTable localsTable;
    VariableModel model;
    MyTableModel tableModel;
    Evaluator evaluator;
    EvalTextArea cmdLine;
    JSplitPane split;
    JSDebugger db;
    boolean enabled;
    ContextWindow(JSDebugger db) {
	super();
	this.db = db;
	enabled = false;
	JPanel left = new JPanel();
	JToolBar t1 = new JToolBar("Variables");
	t1.setLayout(new GridLayout());
	t1.add(left);
	JPanel p1 = new JPanel();
	p1.setLayout(new GridLayout());
	JPanel p2 = new JPanel();
	p2.setLayout(new GridLayout());
	p1.add(t1);
	JLabel label = new JLabel("Context:");
	context = new JComboBox();
	toolTips = new java.util.Vector();
	label.setBorder(context.getBorder());
	context.addActionListener(this);
	context.setActionCommand("ContextSwitch");
	GridBagLayout layout = new GridBagLayout();
	left.setLayout(layout);
	GridBagConstraints lc = new GridBagConstraints();
	lc.insets.left = 5;
	lc.anchor = GridBagConstraints.WEST;
	lc.ipadx = 5;
	layout.setConstraints(label, lc);
	left.add(label);
	GridBagConstraints c = new GridBagConstraints();
	c.gridwidth = GridBagConstraints.REMAINDER;
	c.fill = GridBagConstraints.HORIZONTAL;
	c.anchor = GridBagConstraints.WEST;
	layout.setConstraints(context, c);
	left.add(context);
	tabs = new JTabbedPane(SwingConstants.BOTTOM);
	tabs.setPreferredSize(new Dimension(500,300));
	thisTable = new MyTreeTable(new AbstractTreeTableModel(new DefaultMutableTreeNode()) {
		public Object getChild(Object parent, int index) {
		    return null;
		}
		public int getChildCount(Object parent) {
		    return 0;
		} 
		public int getColumnCount() {
		    //return 3;
		    return 2;
		}
		public String getColumnName(int column) {
		    switch(column) {
		    case 0:
			return " Name";
		    case 1:
			//return "Type";
			//case 2:
			return " Value";
		    }
		    return null;
		}
		public Object getValueAt(Object node, int column) {
		    return null;
		}
	    });
	JScrollPane jsp = new JScrollPane(thisTable);
	jsp.getViewport().setViewSize(new Dimension(5,2));
	tabs.add("this", jsp);
	localsTable = new MyTreeTable(new AbstractTreeTableModel(new DefaultMutableTreeNode()) {
		public Object getChild(Object parent, int index) {
		    return null;
		}
		public int getChildCount(Object parent) {
		    return 0;
		} 
		public int getColumnCount() {
		    return 2;
		}
		public String getColumnName(int column) {
		    switch(column) {
		    case 0:
			return " Name";
		    case 1:
			return " Value";
		    }
		    return null;
		}
		public Object getValueAt(Object node, int column) {
		    return null;
		}
	    });
	localsTable.setAutoResizeMode(JTable.AUTO_RESIZE_ALL_COLUMNS);
	localsTable.setPreferredSize(null);
	jsp = new JScrollPane(localsTable);
	tabs.add("Locals", jsp);
	c.weightx  = c.weighty = 1;
	c.gridheight = GridBagConstraints.REMAINDER;
	c.fill = GridBagConstraints.BOTH;
	c.anchor = GridBagConstraints.WEST;
	layout.setConstraints(tabs, c);
	left.add(tabs);
	evaluator = new Evaluator(db);
	cmdLine = new EvalTextArea(db);
	cmdLine.requestFocus();
	tableModel = evaluator.tableModel;
	jsp = new JScrollPane(evaluator);
	JToolBar t2 = new JToolBar("Evaluate"); 
	tabs2 = new JTabbedPane(SwingConstants.BOTTOM);
	tabs2.add("Watch", jsp);
	tabs2.add("Evaluate", new JScrollPane(cmdLine));
	tabs2.setPreferredSize(new Dimension(500,300));
	t2.setLayout(new GridLayout());
	t2.add(tabs2);
	p2.add(t2);
	evaluator.setAutoResizeMode(JTable.AUTO_RESIZE_ALL_COLUMNS);
	split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, 
				p1,
				p2);
	split.setOneTouchExpandable(true);
	split.setResizeWeight(0.5);
	setLayout(new BorderLayout());
	add(split, BorderLayout.CENTER);

	final JToolBar finalT1 = t1;
	final JToolBar finalT2 = t2;
	final JPanel finalP1 = p1;
	final JPanel finalP2 = p2;
	final JSplitPane finalSplit = split;
	final JPanel finalThis = this;
	HierarchyListener listener = new HierarchyListener() {
		public void hierarchyChanged(HierarchyEvent e) {
		    Component thisParent = finalThis.getParent();
		    if(thisParent == null) {
			return;
		    }
		    Component parent = finalT1.getParent();
		    boolean leftDocked = true;
		    boolean rightDocked = true;
		    if(parent != null) {
			if(parent != finalP1) {
			    while(!(parent instanceof JFrame)) {
				parent = parent.getParent();
			    }
			    JFrame frame = (JFrame)parent;
			    frame.setResizable(true);
			    leftDocked = false;
			} else {
			    leftDocked = true;
			}
		    }
		    parent = finalT2.getParent();
		    if(parent != null) {
			if(parent != finalP2) {
			    while(!(parent instanceof JFrame)) {
				parent = parent.getParent();
			    }
			    JFrame frame = (JFrame)parent;
			    frame.setResizable(true);
			    rightDocked = false;
			} else {
			    rightDocked = true;
			}
		    }
		    JSplitPane split = (JSplitPane)thisParent;
		    if(leftDocked) {
			if(rightDocked) {
			    finalSplit.setDividerLocation(0.5);
			} else {
			    finalSplit.setDividerLocation(1.0);
			}
			split.setDividerLocation(0.66);
		    } else if(rightDocked) {
			    finalSplit.setDividerLocation(0.0);
			    split.setDividerLocation(0.66);
		    } else {
			// both undocked
			split.setDividerLocation(1.0);
		    }
		}
	    };
	t1.addHierarchyListener(listener);
	t2.addHierarchyListener(listener);
	disable();
    }
    
    public void actionPerformed(ActionEvent e) {
	if(!enabled) return;
	if(e.getActionCommand().equals("ContextSwitch")) {
	    Context cx = Context.getCurrentContext();
	    int frameIndex = context.getSelectedIndex();
	    context.setToolTipText(toolTips.elementAt(frameIndex).toString());
	    Scriptable obj;
	    int frameCount = cx.getFrameCount();
	    if(frameIndex < frameCount) {
		obj = cx.getFrame(frameIndex).getVariableObject();
	    } else {
		return;
	    }
	    NativeCall call = null;
	    if(obj instanceof NativeCall) {
		call = (NativeCall)obj;
		obj = call.getThisObj();
	    }
	    JTree tree = thisTable.resetTree(model = new VariableModel(obj));
	    if(call == null) {
		tree = localsTable.resetTree(new AbstractTreeTableModel(new DefaultMutableTreeNode()) {
			public Object getChild(Object parent, int index) {
			    return null;
			}
			public int getChildCount(Object parent) {
			    return 0;
			} 
			public int getColumnCount() {
			    return 2;
			}
			public String getColumnName(int column) {
			    switch(column) {
			    case 0:
				return " Name";
			    case 1:
				return " Value";
			    }
			    return null;
			}
			public Object getValueAt(Object node, int column) {
			    return null;
			}
		    });
	    } else {
		tree = localsTable.resetTree(model = new VariableModel(call));
	    }
	    db.contextSwitch(frameIndex);
	    tableModel.updateModel();
	}
    }

    public void disable() {
	context.setEnabled(false);
	thisTable.setEnabled(false);
	localsTable.setEnabled(false);
	evaluator.setEnabled(false);
	cmdLine.setEnabled(false);
    }

    public void enable() {
	context.setEnabled(true);
	thisTable.setEnabled(true);
	localsTable.setEnabled(true);
	evaluator.setEnabled(true);
	cmdLine.setEnabled(true);
    }
    
    public void disableUpdate() {
	enabled = false;
    }

    public void enableUpdate() {
	enabled = true;
    }
};

class CreateFileWindow implements Runnable {


    JSDebugger db;
    String url;
    String text;
    int line;
    boolean activate;

    CreateFileWindow(JSDebugger db,
		     String url, String text, int line) {
	this.db = db;
	this.url = url;
	this.text = text;
	this.line = line;
	this.activate = true;
    }

    CreateFileWindow(JSDebugger db,
		     String url, String text, int line, boolean activate) {
	this.db = db;
	this.url = url;
	this.text = text;
	this.line = line;
	this.activate = activate;
    }

    public void run() {
	FileWindow w = new FileWindow(db, url, text);
	db.fileWindows.put(url, w);
	if(db.currentWindow != null) {
	    db.currentWindow.setPosition(-1);
	}
	try {
	    w.setPosition(w.textArea.getLineStartOffset(line-1));
	} catch(BadLocationException exc) {
	    w.setPosition(-1);
	}
	w.setVisible(true);
	db.desk.add(w);
	db.currentWindow = w;
	db.menubar.addFile(url);
	if(activate) {
	    try {
		w.setMaximum(true);
		w.show();
		w.setSelected(true);
	    } catch(Exception exc) {
	    }
	}
    }
}

class SetFilePosition implements Runnable {

    JSDebugger db;
    FileWindow w;
    int line;
    boolean activate;

    SetFilePosition(JSDebugger db, FileWindow w, int line) {
	this.db = db; 
	this.w = w; 
	this.line = line; 
	activate = true;
    }

    SetFilePosition(JSDebugger db, FileWindow w, int line, boolean activate) {
	this.db = db; 
	this.w = w; 
	this.line = line; 
	this.activate = activate;
    }

    public void run() {
	JTextArea ta = w.textArea;
	try {
	    int loc = ta.getLineStartOffset(line-1);
	    if(db.currentWindow != null && db.currentWindow != w) {
		db.currentWindow.setPosition(-1);
	    }
	    w.setPosition(loc);
	    db.currentWindow = w;
	} catch(BadLocationException exc) {
	    // fix me
	} 
	if(activate) {
	    if(w.isIcon()) {
		db.desk.getDesktopManager().deiconifyFrame(w);
	    }
	    db.desk.getDesktopManager().activateFrame(w);
	    try {
		w.show();
		w.setSelected(true);
	    } catch(Exception exc) {
	    }
	}
    }
}

class SetFileText implements Runnable {

    FileWindow w;
    String text;

    SetFileText(FileWindow w, String text) {
	this.w = w; this.text = text;
    }

    public void run() {
	w.setText(text);
    }
}

class UpdateContext implements Runnable {
    JSDebugger db;
    Context cx;
    UpdateContext(JSDebugger db, Context cx) {
	this.db = db;
	this.cx = cx;
    }

    public void run() {
	db.context.enable();
	JComboBox ctx = db.context.context;
	Vector toolTips = db.context.toolTips;
	db.context.disableUpdate();
	int frameCount = cx.getFrameCount();
	ctx.removeAllItems();
	toolTips.clear();
	for(int i = 0; i < frameCount; i++) {
	    org.mozilla.javascript.debug.Frame frame = cx.getFrame(i);
	    String sourceName = frame.getSourceName();
	    String location;
	    String shortName = sourceName;
	    int lineNumber = frame.getLineNumber();
	    if(sourceName == null) {
		if(lineNumber == -1) {
		    shortName = sourceName = "<eval>";
		} else {
		    shortName = sourceName = "<stdin>";
		}
	    } else {
		if(sourceName.length() > 20) {
		    shortName = new File(sourceName).getName();
		}
	    }
	    location = "\"" + shortName + "\", line " + lineNumber;
	    ctx.insertItemAt(location, i);
	    location = "\"" + sourceName + "\", line " + lineNumber;
	    toolTips.addElement(location);
	}
	db.context.enableUpdate();
	ctx.setSelectedIndex(0);
	ctx.setMinimumSize(new Dimension(50, ctx.getMinimumSize().height));
    }
};

class Menubar extends JMenuBar implements ActionListener {
    JMenu getDebugMenu() {
	return getMenu(2);
    }
    Menubar(JSDebugger db) {
	super();
	this.db = db;
	String[] fileItems  = {"Load...", "Exit"};
	String[] fileCmds  = {"Load", "Exit"};
	char[] fileShortCuts = {'L', 'X'};
	String[] editItems = {"Cut", "Copy", "Paste", "Go to function..."};
	char[] editShortCuts = {'T', 'C', 'P', 'F'};
	String[] debugItems = {"Break", "Go", "Step Into", "Step Over", "Step Out"};
	char[] debugShortCuts = {'B', 'G', 'I', 'O', 'T'};
	String[] plafItems = {"Metal", "Windows", "Motif"};
	char [] plafShortCuts = {'M', 'W', 'F'};
	int[] debugAccelerators = {KeyEvent.VK_PAUSE, 
				   KeyEvent.VK_F5, 
				   KeyEvent.VK_F11, 
				   KeyEvent.VK_F7,
				   KeyEvent.VK_F8,
				   0, 0};
				    
	JMenu fileMenu = new JMenu("File");
	fileMenu.setMnemonic('F');
	JMenu editMenu = new JMenu("Edit");
	editMenu.setMnemonic('E');
	JMenu plafMenu = new JMenu("Platform");
	plafMenu.setMnemonic('P');
	JMenu debugMenu = new JMenu("Debug");
	debugMenu.setMnemonic('D');
	windowMenu = new JMenu("Window");
	windowMenu.setMnemonic('W');
	for(int i = 0; i < fileItems.length; ++i) {
	    JMenuItem item = new JMenuItem(fileItems[i],
					   fileShortCuts[i]);
	    item.setActionCommand(fileCmds[i]);
	    item.addActionListener(this);
	    fileMenu.add(item);
	}
	for(int i = 0; i < editItems.length; ++i) {
	    JMenuItem item = new JMenuItem(editItems[i],
					   editShortCuts[i]);
	    item.addActionListener(this);
	    editMenu.add(item);
	}
	for(int i = 0; i < plafItems.length; ++i) {
	    JMenuItem item = new JMenuItem(plafItems[i],
					   plafShortCuts[i]);
	    item.addActionListener(this);
	    plafMenu.add(item);
	}
	for(int i = 0; i < debugItems.length; ++i) {
	    JMenuItem item = new JMenuItem(debugItems[i],
					   debugShortCuts[i]);
	    item.addActionListener(this);
	    if(debugAccelerators[i] != 0) {
		KeyStroke k = KeyStroke.getKeyStroke(debugAccelerators[i], 0);
		item.setAccelerator(k);
	    }
	    if(i != 0) {
		item.setEnabled(false);
	    }
	    debugMenu.add(item);
	}
	add(fileMenu);
	add(editMenu);
	//add(plafMenu);
	add(debugMenu);
	JMenuItem item;
	windowMenu.add(item = new JMenuItem("Cascade", 'A'));
	item.addActionListener(this);
	windowMenu.add(item = new JMenuItem("Tile", 'T'));
	item.addActionListener(this);
	windowMenu.addSeparator();
	windowMenu.add(item = new JMenuItem("Console", 'C'));
	item.addActionListener(this);
	add(windowMenu);

    }

    public void actionPerformed(ActionEvent e) {
	String cmd = e.getActionCommand();
	String plaf_name = null;
	if(cmd.equals("Metal")) {
	    plaf_name = "javax.swing.plaf.metal.MetalLookAndFeel";
	} else if(cmd.equals("Windows")) {
	    plaf_name = "com.sun.java.swing.plaf.windows.WindowsLookAndFeel";
	} else if(cmd.equals("Motif")) {
	    plaf_name = "com.sun.java.swing.plaf.motif.MotifLookAndFeel";
	} else {
	    db.actionPerformed(e);
	    return;
	}
	try {
	    UIManager.setLookAndFeel(plaf_name);
	    SwingUtilities.updateComponentTreeUI(db);
	    SwingUtilities.updateComponentTreeUI(db.dlg);
	} catch(Exception ignored) {
	    //ignored.printStackTrace();
	}
    }

    public void addFile(String fileName) {
	int count = windowMenu.getItemCount();
	JMenuItem item;
	if(count == 4) {
	    windowMenu.addSeparator();
	    count++;
	}
	if(count - 4 == 10) {
	    windowMenu.add(item = new JMenuItem("More Windows...", 'M'));
	    item.setActionCommand("More Windows...");
	    item.addActionListener(this);
	    return;
	} else if(count - 4 < 10) {
	    windowMenu.add(item = new JMenuItem((char)('0' + (count-4)) + " " + fileName, '0' + (count - 4)));
	} else {
	    return;
	}
	item.setActionCommand(fileName);
	item.addActionListener(this);
    }

    JSDebugger db;
    JMenu windowMenu;
};

class EnterInterrupt implements Runnable {
    JSDebugger db;
    Context cx;
    EnterInterrupt(JSDebugger db, Context cx) {
	this.db = db;
	this.cx = cx;
    }
    public void run() {
	Context newCx;
	if((newCx = Context.enter(cx)) != cx) {
	    System.out.println("debugger error: Enter Interrupt: failed to obtain context: " + cx);
	}
	JMenu menu = db.getJMenuBar().getMenu(0); 
	menu.getItem(0).setEnabled(false); // File->Load
	menu = db.getJMenuBar().getMenu(2);
	menu.getItem(0).setEnabled(false); // Debug->Break
	int count = menu.getItemCount();
	for(int i = 1; i < count; ++i) {
	    menu.getItem(i).setEnabled(true);
	}
	boolean b = false;
	for(int ci = 0, cc = db.toolBar.getComponentCount(); ci < cc; ci++) {
	    db.toolBar.getComponent(ci).setEnabled(b);
	    b = true;
	}
	db.toolBar.setEnabled(true);
    }
};

class ExitInterrupt implements Runnable {
    JSDebugger db;
    ExitInterrupt(JSDebugger db) {
	this.db = db;
    }
    public void run() {
	JMenu menu = db.getJMenuBar().getMenu(0); 
	menu.getItem(0).setEnabled(true); // File->Load
	menu = db.getJMenuBar().getMenu(2);
	menu.getItem(0).setEnabled(true); // Debug->Break
	int count = menu.getItemCount();
	for(int i = 1; i < count; ++i) {
	    menu.getItem(i).setEnabled(false);
	}
	db.context.disable();
	boolean b = true;
	for(int ci = 0, cc = db.toolBar.getComponentCount(); ci < cc; ci++) {
	    db.toolBar.getComponent(ci).setEnabled(b);
	    b = false;
	}
	db.console.consoleTextArea.requestFocus();
    }
};

class LoadFile implements Runnable {
    Scriptable scope;
    JSDebugger debugger;
    String fileName;
    LoadFile(JSDebugger debugger, Scriptable scope, String fileName) {
	this.scope = scope;
	this.fileName = fileName;
	this.debugger = debugger;
    }
    public void run() {
	Context cx = Context.enter();
	cx.setBreakNextLine(true);
	try {
	    cx.evaluateReader(scope, new FileReader(fileName),
			      fileName, 1, null);
	} catch(Exception exc) {
	    exc.printStackTrace(Main.getErr());
	}
	cx.exit();
    }
}


public class JSDebugger extends JFrame implements Debugger, ContextListener {

    /* ContextListener interface */

    java.util.HashSet contexts = new java.util.HashSet();

    public void contextCreated(Context cx) {
	cx.setDebugger(this);
	cx.setGeneratingDebug(true);
	cx.setOptimizationLevel(-1);
    }
    
    public void contextEntered(Context cx) {
	// If the debugger is attached to cx
	// keep a reference to it even if it was detached
	// from its thread (we cause that to happen below
	// in interrupted)
	if(!contexts.contains(cx)) {
	    if(cx.getDebugger() == this) {
		contexts.add(cx);
	    }
	}
    }
    
    public void contextExited(Context cx) {
    }
    
    public void contextReleased(Context cx) {
	contexts.remove(cx);
    }

    /* end ContextListener interface */
    
    public void doBreak() {
	synchronized(contexts) {
	    Iterator iter = contexts.iterator();
	    while(iter.hasNext()) {
		Context cx = (Context)iter.next();
		cx.setBreakNextLine(true);
	    }
	}
    }

    public void setVisible(boolean b) {
	super.setVisible(b);
	if(b) {
	    // this needs to be done after the window is visible
	    context.split.setDividerLocation(0.5);
	}
    }

    static final int STEP_OVER = 0;
    static final int STEP_INTO = 1;
    static final int STEP_OUT = 2;
    static final int GO = 3;
    static final int BREAK = 4;
    static final int RUN_TO_CURSOR = 5;

    class ThreadState {
	private int stopAtFrameDepth = -1;
    };

    private Hashtable threadState = new Hashtable();
    private Thread runToCursorThread;
    private int runToCursorLine;
    private String runToCursorFile;
    private Hashtable sourceNames = new Hashtable();

    class SourceEntry {
        SourceEntry(StringBuffer source, DebuggableScript fnOrScript) {
            this.source = source;
            this.fnOrScript = fnOrScript;
        }
        StringBuffer source;
        DebuggableScript fnOrScript;
    };

    Hashtable functionMap = new Hashtable();
    Hashtable breakpointsMap = new Hashtable();
    
    public void handleCompilationDone(Context cx, DebuggableScript fnOrScript, 
                                      StringBuffer source) {
        String sourceName = fnOrScript.getSourceName();
	//System.out.println("sourceName = " + sourceName);
        if (sourceName != null && !sourceName.equals("<stdin>")) {
	    try {
		sourceName = new File(sourceName).getCanonicalPath();
	    } catch(IOException exc) {
	    }
	} else {
	    Enumeration e = fnOrScript.getLineNumbers();
	    if(!e.hasMoreElements() || ((Integer)e.nextElement()).intValue() == -1) {
		sourceName = "<eval>";
	    } else {
		sourceName = "<stdin>";
	    } 
	}
	Vector v = (Vector) sourceNames.get(sourceName);
	if (v == null) {
	    v = new Vector();
	    sourceNames.put(sourceName, v);
	}
	SourceEntry entry = new SourceEntry(source, fnOrScript);
	v.addElement(entry); 
	if(fnOrScript.getScriptable() instanceof NativeFunction) {
	    NativeFunction f = (NativeFunction)fnOrScript.getScriptable();
	    String name = f.jsGet_name();
	    if(name.length() > 0 && !name.equals("anonymous")) {
		functionMap.put(name, entry);
	    }
	}
	loadedFile(sourceName, source.toString());
    }

    public void handleBreakpointHit(Context cx) {
	interrupted(cx);
    }
    
    public void handleExceptionThrown(Context cx, Object e) {
	if(cx.getFrameCount() == 0) {
	    org.mozilla.javascript.debug.Frame frame = cx.getFrame(0);  
	    String sourceName = frame.getSourceName();
	    int lineNumber = frame.getLineNumber();
	    FileWindow w = null;
	    if(sourceName == null) {
		if(lineNumber == -1) {
		    sourceName = "<eval>";
		} else {
		    sourceName = "<stdin>";
		}
	    } else {
		w = getFileWindow(sourceName);
	    }
	    if(e instanceof NativeJavaObject) {
		e = ((NativeJavaObject)e).unwrap();
	    }
	    String msg = e.toString();
	    if(msg == null || msg.length() == 0) {
		msg = e.getClass().toString();
	    }
	    msg += " (" + sourceName + ", line " + lineNumber + ")";
	    if(w != null) {
		//swingInvoke(new SetFilePosition(this, w, lineNumber));
	    }
	    MessageDialogWrapper.showMessageDialog(this,
						   msg,
						   "Exception in Script",
						   JOptionPane.ERROR_MESSAGE);
	}
    }
    
    JDesktopPane desk;
    ContextWindow context;
    Menubar menubar;
    JToolBar toolBar;
    JSInternalConsole console;
    EvalWindow evalWindow;
    JSplitPane split1;
    JLabel statusBar;

    public JSDebugger(String name) {
	super(name);
	setJMenuBar(menubar = new Menubar(this));
	toolBar = new JToolBar();
	JButton button;
	toolBar.add(button = new JButton("Break"));
	button.setToolTipText("Break");
	button.setActionCommand("Break");
	button.addActionListener(menubar);
	button.setEnabled(true);
	toolBar.add(button = new JButton("Go"));
	button.setToolTipText("Go");
	button.setActionCommand("Go");
	button.addActionListener(menubar);
	button.setEnabled(false);
	toolBar.add(button = new JButton("Step Into"));
	button.setToolTipText("Step Into");
	button.setActionCommand("Step Into");
	button.addActionListener(menubar);
	button.setEnabled(false);
	toolBar.add(button = new JButton("Step Over"));
	button.setToolTipText("Step Over");
	button.setActionCommand("Step Over");
	button.setEnabled(false);
	button.addActionListener(menubar);
	toolBar.add(button = new JButton("Step Out"));
	button.setToolTipText("Step Out");
	button.setActionCommand("Step Out");
	button.setEnabled(false);
	button.addActionListener(menubar);
	//toolBar.add(button = new JButton(new String("Run to Cursor")));
	//button.setToolTipText("Run to Cursor");
	//button.setActionCommand("Run to Cursor");
	//button.setEnabled(false);
	//button.addActionListener(menubar);
	// getContentPane().setLayout(new BorderLayout());
	getContentPane().add(toolBar, BorderLayout.NORTH);
	desk = new JDesktopPane();
	desk.setPreferredSize(new Dimension(600, 300));
	desk.setMinimumSize(new Dimension(150, 50));
	desk.add(console = new JSInternalConsole("JavaScript Console"));
	desk.getDesktopManager().activateFrame(console);

	context = new ContextWindow(this);
	context.setPreferredSize(new Dimension(600, 120));
	context.setMinimumSize(new Dimension(50, 50));
	split1 = new JSplitPane(JSplitPane.VERTICAL_SPLIT, desk,
					  context);
	split1.setOneTouchExpandable(true);
	split1.setResizeWeight(0.66);
	getContentPane().add(split1, BorderLayout.CENTER);
	statusBar = new JLabel();
	statusBar.setText("Thread: ");
	getContentPane().add(statusBar, BorderLayout.SOUTH);
	dlg = new JFileChooser();
	dlg.setDialogTitle("Select a file to load");
        javax.swing.filechooser.FileFilter filter = 
            new javax.swing.filechooser.FileFilter() {
                    public boolean accept(File f) {
                        if(f.isDirectory()) {
                            return true;
                        }
                        String n = f.getName();
                        int i = n.lastIndexOf('.');
                        if(i > 0 && i < n.length() -1) {
                            String ext = n.substring(i + 1).toLowerCase();
                            if(ext.equals("js")) {
                                return true;
                            }
                        }
                        return false;
                    }
                    
                    public String getDescription() {
                        return "JavaScript Files (*.js)";
                    }
                };
        dlg.addChoosableFileFilter(filter);
	try {
	    console.setMaximum(true);
	    console.setSelected(true);
	} catch(Exception exc) {
	}
    }

    public InputStream getIn() {
	return console.getIn();
    }

    public PrintStream getOut() {
	return console.getOut();
    }

    public PrintStream getErr() {
	return console.getErr();
    }

    public static void main(String[] args) {
	try {
	    final JSDebugger sdb = new JSDebugger("Rhino JavaScript Debugger");
	    swingInvoke(new Runnable() {
		    public void run() {
			sdb.addWindowListener(new WindowAdapter() {
				public void windowClosing(WindowEvent e) {
				    System.exit(0);
				}
			    });
			sdb.pack();
			sdb.setSize(600, 460);
			sdb.setVisible(true);
		    }
		});
	    System.setIn(sdb.getIn());
	    System.setOut(sdb.getOut());
	    System.setErr(sdb.getErr());
	    Context.addContextListener(sdb);
	    String className = java.lang.System.getProperty("rhino.shell");
	    if(className != null) {
		try {
		    Class clazz = Class.forName(className);
		    Method mainMethod = 
			clazz.getMethod("main", new Class[] {String[].class});
		    mainMethod.invoke(null, new Object[] {args});
		    return;
		} catch(Exception exc) {
		}
	    }
	    Main.main(args);
	} catch(Exception exc) {
	    exc.printStackTrace();
	}
    }

    public FileWindow getFileWindow(String fileName) {
	if(fileName == null || fileName.equals("<stdin>") || fileName.equals("<eval>")) {
	    return null;
	}
	try {
	    // Can't do  this if we're remote debugging
	    String file = new File(fileName).getCanonicalPath();
	    Enumeration e = fileWindows.keys();
	    for(; e.hasMoreElements(); ) {
		String name = (String)e.nextElement();
		if(file.equals(new File(name).getCanonicalPath())) {
		    FileWindow w = (FileWindow)fileWindows.get(name);
		    w.setUrl(fileName);
		    return w;
		}
	    }
	} catch(IOException exc) {
	}
	return (FileWindow)fileWindows.get(fileName);
    }

    public void loadedFile(String fileName, String text) {
	FileWindow w = getFileWindow(fileName);
	if(w != null) {
	    swingInvoke(new SetFileText(w, text));
	}
    }

    static void swingInvoke(Runnable f) {
	try {
	    SwingUtilities.invokeAndWait(f);
	} catch(Exception exc) {
	    exc.printStackTrace();
	}
    }

    static void swingInvokeLater(Runnable f) {
	try {
	    SwingUtilities.invokeLater(f);
	} catch(Exception exc) {
	    exc.printStackTrace();
	}
    }

    int frameIndex = -1;

    void contextSwitch(int frameIndex) {
	Context cx  = Context.getCurrentContext();
	if(cx != null) {
	    int frameCount = cx.getFrameCount();
	    if(frameIndex < 0 || frameIndex >= frameCount) {
		return;
	    }
	    this.frameIndex = frameIndex;
	    org.mozilla.javascript.debug.Frame frame = cx.getFrame(frameIndex);
	    String sourceName = frame.getSourceName();
	    if(sourceName == null || sourceName.equals("<stdin>")) {
		console.show();
		console.consoleTextArea.requestFocus();
		return;
	    } 
	    if(sourceName == "<eval>") {
		return;
	    }
	    try {
		sourceName = new File(sourceName).getCanonicalPath();
	    } catch(IOException exc) {
	    }
	    int lineNumber = frame.getLineNumber();
	    this.frameIndex = frameIndex;
	    FileWindow w = getFileWindow(sourceName);
	    if(w != null) {
		SetFilePosition action = new SetFilePosition(this, w, lineNumber);
		action.run();
	    } else {
		Vector v = (Vector)sourceNames.get(sourceName);
		String source = ((SourceEntry)v.elementAt(0)).source.toString();
		CreateFileWindow action = new CreateFileWindow(this, 
							       sourceName,
							       source,
							       lineNumber);
		action.run();
	    }
	}
    }
    

    public static void start(Context cx, Scriptable scope) {
    }

    public void interrupted(Context cx) {
	synchronized(debuggerMonitor) {
	    Thread thread = Thread.currentThread();
	    statusBar.setText("Thread: " + thread.toString());
	    ThreadState state = (ThreadState)threadState.get(thread);
	    int stopAtFrameDepth = -1;
	    if(state != null) {
		stopAtFrameDepth = state.stopAtFrameDepth;
	    }
	    if(runToCursorFile != null && thread == runToCursorThread) {
		int frameCount = cx.getFrameCount();
		if(frameCount > 0) {
		    org.mozilla.javascript.debug.Frame frame = cx.getFrame(0);  
		    String sourceName = frame.getSourceName();
		    if(sourceName != null) {
			try {
			    sourceName = new File(sourceName).getCanonicalPath();
			} catch(IOException exc) {
			}
			if(sourceName.equals(runToCursorFile)) {
			    int lineNumber = frame.getLineNumber();
			    if(lineNumber == runToCursorLine) {
				stopAtFrameDepth = -1;
				runToCursorFile = null;
			    } else {
				FileWindow w = getFileWindow(sourceName);
				if(w == null ||
				   !w.isBreakPoint(lineNumber)) { 
				    return;
				} else {
				    runToCursorFile = null;
				}
			    }
			} 
		    }
		} else {
		}
	    }
	    if(stopAtFrameDepth > 0) {
		if (cx.getFrameCount() > stopAtFrameDepth)
		    return;
	    }
	    if(state != null) {
		state.stopAtFrameDepth = -1;
	    }
	    int frameCount = cx.getFrameCount();
	    this.frameIndex = frameCount -1;
	    int line = 0;
	    if(frameCount == 0) {
		return;
	    }
	    org.mozilla.javascript.debug.Frame frame = cx.getFrame(0);
	    String fileName = frame.getSourceName();
	    if(fileName != null && !fileName.equals("<stdin>")) {
		try {
		    fileName = new File(fileName).getCanonicalPath();
		} catch(IOException exc) {
		}
	    }
	    cx.setBreakNextLine(false);
	    line = frame.getLineNumber();
	    int enterCount = 0;
	    cx.exit();
	    while(Context.getCurrentContext() != null) {
		Context.exit();
		enterCount++;
	    }
	    if(fileName != null && !fileName.equals("<stdin>")) {
		FileWindow w = (FileWindow)getFileWindow(fileName);
		if(w != null) {
		    SetFilePosition action = new SetFilePosition(this, w, line);
		    swingInvoke(action);
		} else {
		    Vector v = (Vector)sourceNames.get(fileName);
		    String source = ((SourceEntry)v.elementAt(0)).source.toString();
		    CreateFileWindow action = new CreateFileWindow(this, 
								   fileName,
								   source,
								   line);
		    swingInvoke(action);
		}
	    } else {
		final JInternalFrame finalConsole = console;
		swingInvoke(new Runnable() {
			public void run() {
			    finalConsole.show();
			}
		    });
	    }
	    swingInvoke(new EnterInterrupt(this, cx));
	    swingInvoke(new UpdateContext(this, cx));
	    int returnValue;
	    synchronized(monitor) {
		this.returnValue = -1;
		try {
		    while(this.returnValue == -1) {
			monitor.wait();
		    }
		    returnValue = this.returnValue;
		} catch(InterruptedException exc) {
		    return;
		}
	    }
	    final Context finalContext = cx;
	    swingInvoke(new Runnable() {
		    public void run() {
			finalContext.exit();
		    }
		});
	    swingInvoke(new ExitInterrupt(this));
	    Context current;
	    if((current = Context.enter(cx)) != cx) {
		System.out.println("debugger error: cx = " + cx + " current = " + current);
		
	    }
	    while(enterCount > 0) {
		Context.enter();
		enterCount--;
	    }
	    switch(returnValue) {
	    case STEP_OVER:
		cx.setBreakNextLine(true);
		stopAtFrameDepth = cx.getFrameCount();
		if(state == null) {
		    state = new ThreadState();
		    threadState.put(thread, state);
		}
		state.stopAtFrameDepth = stopAtFrameDepth;
		break;
	    case STEP_INTO:
		cx.setBreakNextLine(true);
		if(state != null) {
		    state.stopAtFrameDepth = -1;
		}
		break;
	    case STEP_OUT:
		cx.setBreakNextLine(true);
		if(state == null) {
		    state = new ThreadState();
		    threadState.put(thread, state);
		}
		state.stopAtFrameDepth = cx.getFrameCount() - 1;
		break;
	    case RUN_TO_CURSOR:
		cx.setBreakNextLine(true);
		if(state != null) {
		    state.stopAtFrameDepth = -1;
		}
		break;
	    }
	}
    }

    JFileChooser dlg;

    public String chooseFile() {
	File CWD = null;
	String dir = System.getProperty("user.dir");
	if(dir != null) {
	    CWD = new File(dir);
	}
	if(CWD != null) {
	    dlg.setCurrentDirectory(CWD);
	}
	int returnVal = dlg.showOpenDialog(this);
	if(returnVal == JFileChooser.APPROVE_OPTION) {
	    String result = dlg.getSelectedFile().getPath();
	    CWD = dlg.getSelectedFile().getParentFile();
	    java.lang.System.setProperty("user.dir", CWD.getPath());
	    return result;
	}
	return null;
    }

    public void actionPerformed(ActionEvent e) {
	String cmd = e.getActionCommand();
	int returnValue = -1;
	if(cmd.equals("Step Over")) {
	    returnValue = STEP_OVER;
	} else if(cmd.equals("Step Into")) {
	    returnValue = STEP_INTO;
	} else if(cmd.equals("Step Out")) {
	    returnValue = STEP_OUT;
	} else if(cmd.equals("Go")) {
	    returnValue = GO;
	} else if(cmd.equals("Break")) {
	    doBreak();
	} else if(cmd.equals("Run to Cursor")) {
	    returnValue = RUN_TO_CURSOR;
	} else if(cmd.equals("Exit")) {
	    System.exit(0);
	} else if(cmd.equals("Load")) {
	    String fileName = chooseFile();
	    if(fileName != null) {
		new Thread(new LoadFile(this, Main.getScope(),
					fileName)).start();
	    }
	} else if(cmd.equals("More Windows...")) {
	    MoreWindows dlg = new MoreWindows(this, fileWindows, "Window", "Files");
	    dlg.showDialog(this);
	} else if(cmd.equals("Console")) {
	    if(console.isIcon()) {
		desk.getDesktopManager().deiconifyFrame(console);
	    }
	    console.show();
	    try {
		console.setSelected(true);
	    } catch(Exception exc) {
	    }
	    console.consoleTextArea.requestFocus();
	} else if(cmd.equals("Cut")) {
	} else if(cmd.equals("Copy")) {
	} else if(cmd.equals("Paste")) {
	} else if(cmd.equals("Go to function...")) {
	    FindFunction dlg = new FindFunction(this, functionMap, "Go to function", "Function");
	    dlg.showDialog(this);
	} else if(cmd.equals("Tile")) {
	    JInternalFrame[] frames = desk.getAllFrames();
	    int count = frames.length;
	    int rows, cols;
	    rows = cols = (int)Math.sqrt(count);
	    if(rows*cols < count) {
		cols++;
		if(rows * cols < count) {
		    rows++;
		}
	    }
	    Dimension size = desk.getSize();
	    int w = size.width/cols;
	    int h = size.height/rows;
	    int x = 0;
	    int y = 0;
	    for(int i = 0; i < rows; i++) {
		for(int j = 0; j < cols; j++) {
		    int index = (i*cols) + j;
		    if(index >= frames.length) {
			break;
		    }
		    JInternalFrame f = frames[index];
		    try {
			f.setIcon(false);
			f.setMaximum(false);
		    } catch (Exception exc) {
		    }
		    desk.getDesktopManager().setBoundsForFrame(f, x, y, w, h);
		    x += w;
		}
		y += h;
		x = 0;
	    }
    	} else if(cmd.equals("Cascade")) {
	    JInternalFrame[] frames = desk.getAllFrames();
	    int count = frames.length;
	    int x, y, w, h;
	    x = y = 0;
	    h = desk.getHeight();
	    int d = h / count;
	    if(d > 30) d = 30;
	    for(int i = count -1; i >= 0; i--, x += d, y += d) {
		JInternalFrame f = frames[i];
		try {
		    f.setIcon(false);
		    f.setMaximum(false);
		} catch (Exception exc) {
		}
		Dimension dimen = f.getPreferredSize();
		w = dimen.width;
		h = dimen.height;
		desk.getDesktopManager().setBoundsForFrame(f, x, y, w, h);
	    }
	} else {
	    Object obj = getFileWindow(cmd);
	    if(obj != null) {
		FileWindow w = (FileWindow)obj;
		if(w.isIcon()) {
		    desk.getDesktopManager().deiconifyFrame(w);
		}
		w.show();
		try {
		    w.setSelected(true);
		} catch(Exception exc) {
		}
	    }
	}
	if(returnValue != -1) {
	    if(currentWindow != null) currentWindow.setPosition(-1);
	    synchronized(monitor) {
		this.returnValue = returnValue;
		monitor.notify();
	    }
	}
    }

    void runToCursor(String fileName,
		     int lineNumber,
		     ActionEvent evt) {
	Vector v = (Vector) sourceNames.get(fileName);
	if(v == null) {
	    System.out.println("debugger error: Couldn't find source: " + fileName);
	}
	int i;
	SourceEntry se = null;
	for (i = v.size() -1; i >= 0; i--) {
	    se = (SourceEntry) v.elementAt(i);
	    if (se.fnOrScript.removeBreakpoint(lineNumber)) {
		se.fnOrScript.placeBreakpoint(lineNumber);
		break;
	    } else if(se.fnOrScript.placeBreakpoint(lineNumber)) {
		se.fnOrScript.removeBreakpoint(lineNumber);
		break;
	    }
	}
	if(i >= 0) {
	    runToCursorFile = fileName;
	    runToCursorLine = lineNumber;
	    actionPerformed(evt);
	}
    }

    int setBreakPoint(String sourceName, int lineNumber) {
	Vector v = (Vector) sourceNames.get(sourceName);
	if(v == null) return -1;
	int i=0;
	int result = -1;
	for (i = v.size() -1; i >= 0; i--) {
	    SourceEntry se = (SourceEntry) v.elementAt(i);
	    if (se.fnOrScript.placeBreakpoint(lineNumber)) {
		result = lineNumber;
	    }
	}
	return result;
    }

    void clearBreakPoint(String sourceName, int lineNumber) {
	Vector v = (Vector) sourceNames.get(sourceName);
	if(v == null) return;
	int i=0;
	for (; i < v.size(); i++) {
	    SourceEntry se = (SourceEntry) v.elementAt(i);
	    se.fnOrScript.removeBreakpoint(lineNumber);
	}
    }

    JMenu getWindowMenu() {
	return menubar.getMenu(3);
    }

    public void removeWindow(FileWindow w) {
	fileWindows.remove(w.getUrl());
	JMenu windowMenu = getWindowMenu();
	int count = windowMenu.getItemCount();
	JMenuItem lastItem = windowMenu.getItem(count -1);
	for(int i = 5; i < count; i++) {
	    JMenuItem item = windowMenu.getItem(i);
	    if(item == null) continue; // separator
	    String text = item.getText();
	    //1 D:\foo.js
	    //2 D:\bar.js
	    int pos = text.indexOf(' ');
	    if(text.substring(pos + 1).equals(w.getUrl())) {
		windowMenu.remove(item);
		// Cascade    [0]
		// Tile       [1]
		// -------    [2]
		// Console    [3]
		// -------    [4]
		if(count == 6) {
		    // remove the final separator
		    windowMenu.remove(4);
		} else {
		    int j = i - 4;
		    for(;i < count -2; i++) {
			JMenuItem thisItem = windowMenu.getItem(i);
			if(thisItem != null) {
			    //1 D:\foo.js
			    //2 D:\bar.js
			    text = thisItem.getText();
			    pos = text.indexOf(' ');
			    thisItem.setText((char)('0' + j) + " " +
					     text.substring(pos + 1));
			    thisItem.setMnemonic('0' + j);
			    j++;
			}
		    }
		    if(count - 6 < 10 && lastItem != item) {
			if(lastItem.getText().equals("More Windows...")) {
			    windowMenu.remove(lastItem);
			}
		    }
		}
		break;
	    }
	}
	windowMenu.revalidate();
    }


    public String eval(String expr) {
	Context cx = Context.getCurrentContext();
	if(cx == null) return "undefined";
	if(frameIndex >= cx.getFrameCount()) {
	    return "undefined";
	}
	String resultString;
	cx.setDebugger(null);
	cx.setGeneratingDebug(false);
	cx.setOptimizationLevel(-1);
	boolean breakNextLine = cx.getBreakNextLine();
	cx.setBreakNextLine(false);
	try {
	    Scriptable scope;
	    scope = cx.getFrame(frameIndex).getVariableObject();
	    Object result;
	    if(scope instanceof NativeCall) {
		NativeCall call = (NativeCall)scope;
		result = NativeGlobal.evalSpecial(cx, call,
						  call.getThisObj(),
						  new Object[]{expr},
						  "", 1);
	    } else {
		result = cx.evaluateString(scope,
				      expr,
				      "",
				      0,
				      null);
	    }
	    try {
		resultString = ScriptRuntime.toString(result);
	    } catch(Exception exc) {
		resultString = result.toString();
	    }
	} catch(Exception exc) {
	    resultString = exc.getMessage();
	}
	cx.setDebugger(this);
	cx.setGeneratingDebug(true);
	cx.setOptimizationLevel(-1);
	cx.setBreakNextLine(breakNextLine);
	return resultString;
    }

    java.util.Hashtable fileWindows = new java.util.Hashtable();
    FileWindow currentWindow;
    Object monitor = new Object();
    Object swingMonitor = new Object();
    Object debuggerMonitor = new Object();
    int returnValue = -1;
};







