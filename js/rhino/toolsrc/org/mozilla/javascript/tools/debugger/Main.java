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
import org.mozilla.javascript.tools.shell.ConsoleTextArea;
import java.util.*;
import java.io.*;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.DefaultTreeCellRenderer;
import java.lang.reflect.Method;

class MessageDialogWrapper {

    static void showMessageDialog(Component parent, String msg, 
				  String title, int flags) {
	if(msg.length() > 60) {
	    StringBuffer buf = new StringBuffer();
	    int len = msg.length();
	    int j = 0;
	    int i;
	    for(i = 0; i < len; i++, j++) {
		char c = msg.charAt(i);
		buf.append(c);
		if(Character.isWhitespace(c)) {
		    int remainder = len - i;
		    int k;
		    for(k = i + 1; k < len; k++) {
			if(Character.isWhitespace(msg.charAt(k))) {
			    break;
			}
		    }
		    if(k < len) {
			int nextWordLen = k - i;
			if(j + nextWordLen > 60) {
			    buf.append('\n');
			    j = 0;
			}
		    }
		} 
	    }
	    msg = buf.toString();
	}
	JOptionPane.showMessageDialog(parent, msg, title, flags);
    }
};

class EvalTextArea extends JTextArea implements KeyListener,
DocumentListener {
    Main db;
    private java.util.Vector history;
    private int historyIndex = -1;
    private int outputMark = 0;

    public void select(int start, int end) {
	//requestFocus();
	super.select(start, end);
    }

    public EvalTextArea(Main db) {
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
	if(db.stringIsCompilableUnit(text)) {
	    if(text.trim().length() > 0) {
	       history.addElement(text);
	       historyIndex = history.size();
	    }
	    append("\n");
	    String result = db.eval(text);
	    if(result.length() > 0) {
		append(result);
		append("\n");
	    }
	    append("% ");
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
        //requestFocus();
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

    public EvalWindow(String name, Main db) {
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
	addInternalFrameListener(new InternalFrameAdapter() {
		public void internalFrameActivated(InternalFrameEvent e) {
		    // hack
		    if(consoleTextArea.hasFocus()) {
			consoleTextArea.getCaret().setVisible(false);
			consoleTextArea.getCaret().setVisible(true);
		    }
		}
	    });
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
	//add(item = new JMenuItem("Run to Cursor"));
	//item.addActionListener(w);
	add(item = new JMenuItem("Run"));
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
	requestFocus();
	getCaret().setVisible(true);
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
	} else if(cmd.equals("Run")) {
	    w.load();
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
  Main db;
  JButton setButton;
  JButton refreshButton;
  JButton cancelButton;

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
	    Main.SourceEntry sourceEntry = (Main.SourceEntry)functionMap.get(value);
	    DebuggableScript script = sourceEntry.fnOrScript;
	    if(script != null) {
		String sourceName = script.getSourceName();
		Enumeration ee = script.getLineNumbers();
		int lineNumber = -1;
		while(ee.hasMoreElements()) {
		    Integer ival = (Integer)ee.nextElement();
		    if(lineNumber == -1) {
			lineNumber = ival.intValue();
		    } else if(ival.intValue() < lineNumber) {
			lineNumber = ival.intValue();
		    }
		}

		FileWindow w = db.getFileWindow(sourceName);
		if(w == null) {
		    (new CreateFileWindow(db, sourceName, sourceEntry.source.toString(), lineNumber)).run();
		    w = db.getFileWindow(sourceName);
		    w.setPosition(-1);
		}
		int start = w.getPosition(lineNumber-1);
		w.select(start, start);
		try {
		    w.show();
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

    FindFunction(Main db, Hashtable functionMap,
		 String title,
		 String labelText) {
	super(db, title, true);
	this.functionMap = functionMap;
	this.db = db;

	cancelButton = new JButton("Cancel");
	setButton = new JButton("Select");
	cancelButton.addActionListener(this);
	setButton.addActionListener(this);
	getRootPane().setDefaultButton(setButton);

	list = new JList(new DefaultListModel());
	DefaultListModel model = (DefaultListModel)list.getModel();
	model.clear();

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

	setButton.setEnabled(a.length > 0);
	list.setSelectionMode(ListSelectionModel.SINGLE_INTERVAL_SELECTION);
	list.addMouseListener(new MouseHandler());
	JScrollPane listScroller = new JScrollPane(list);
	listScroller.setPreferredSize(new Dimension(320, 240));
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
    }
    public void mouseClicked(MouseEvent e) {
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

class FileWindow extends JInternalFrame implements ActionListener {

    Main db;
    FileTextArea textArea;
    FileHeader fileHeader;
    JScrollPane p;
    int currentPos;
    Hashtable breakpoints;
    String url;
    JLabel statusBar;

    public void actionPerformed(ActionEvent e) {
	String cmd = e.getActionCommand();
	if(cmd.equals("Cut")) {
	    // textArea.cut();
	} else if(cmd.equals("Copy")) {
	    textArea.copy();
	} else if(cmd.equals("Paste")) {
	    // textArea.paste();
	}
    }

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

    void load() {
	Scriptable scope = db.getScope();
	if(scope == null) {
	    MessageDialogWrapper.showMessageDialog(db, "Can't load scripts: no scope available", "Run", JOptionPane.ERROR_MESSAGE);
	} else {
	    String fileName = url;
	    if(fileName != null) {
		new Thread(new LoadFile(db,scope,
					fileName)).start();
	    }
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

    FileWindow(Main db, String fileName, String text) {
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
	pack();
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
    Main db;
    Vector expressions;
    Vector values;
    MyTableModel(Main db) {
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
    Evaluator(Main db) {
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
    Main db;
    boolean enabled;
    ContextWindow(Main db) {
	super();
	this.db = db;
	enabled = false;
	JPanel left = new JPanel();
	JToolBar t1 = new JToolBar();
	t1.setName("Variables");
	t1.setLayout(new GridLayout());
	t1.add(left);
	JPanel p1 = new JPanel();
	p1.setLayout(new GridLayout());
	JPanel p2 = new JPanel();
	p2.setLayout(new GridLayout());
	p1.add(t1);
	JLabel label = new JLabel("Context:");
	context = new JComboBox();
	context.setLightWeightPopupEnabled(false);
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
	//cmdLine.requestFocus();
	tableModel = evaluator.tableModel;
	jsp = new JScrollPane(evaluator);
	JToolBar t2 = new JToolBar(); 
	t2.setName("Evaluate");
	tabs2 = new JTabbedPane(SwingConstants.BOTTOM);
	tabs2.add("Watch", jsp);
	tabs2.add("Evaluate", new JScrollPane(cmdLine));
	tabs2.setPreferredSize(new Dimension(500,300));
	t2.setLayout(new GridLayout());
	t2.add(tabs2);
	p2.add(t2);
	evaluator.setAutoResizeMode(JTable.AUTO_RESIZE_ALL_COLUMNS);
	split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, 
			       p1, p2);
	split.setOneTouchExpandable(true);
	Main.setResizeWeight(split, 0.5);
	setLayout(new BorderLayout());
	add(split, BorderLayout.CENTER);

	final JToolBar finalT1 = t1;
	final JToolBar finalT2 = t2;
	final JPanel finalP1 = p1;
	final JPanel finalP2 = p2;
	final JSplitPane finalSplit = split;
	final JPanel finalThis = this;
	final Main finalDb = db;

	ComponentListener clistener = new ComponentListener() {
		boolean t1Docked = true;
		boolean t2Docked = true;
		void check(Component comp) {
		    Component thisParent = finalThis.getParent();
		    if(thisParent == null) {
			return;
		    }
		    Component parent = finalT1.getParent();
		    boolean leftDocked = true;
		    boolean rightDocked = true;
		    boolean adjustVerticalSplit = false;
		    if(parent != null) {
			if(parent != finalP1) {
			    while(!(parent instanceof JFrame)) {
				parent = parent.getParent();
			    }
			    JFrame frame = (JFrame)parent;
			    finalDb.addTopLevel("Variables", frame);

			    // We need the following hacks because:
			    // - We want an undocked toolbar to be
			    //   resizable.
			    // - We are using JToolbar as a container of a 
			    //   JComboBox. Without this JComboBox's popup 
			    //   can get left floating when the toolbar is 
			    //   re-docked.
			    //
			    // We make the frame resizable and then
			    // remove JToolbar's window listener
			    // and insert one of our own that first ensures
			    // the JComboBox's popup window is closed
			    // and then calls JToolbar's window listener.
			    if(!frame.isResizable()) {
				frame.setResizable(true);
				frame.setDefaultCloseOperation(WindowConstants.DO_NOTHING_ON_CLOSE);
				final EventListener[] l = 
				    frame.getListeners(WindowListener.class);
				frame.removeWindowListener((WindowListener)l[0]);
				frame.addWindowListener(new WindowAdapter() {
					public void windowClosing(WindowEvent e) {
					    context.hidePopup();
					    ((WindowListener)l[0]).windowClosing(e);
					}
				    });
				//adjustVerticalSplit = true;
			    }
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
			    finalDb.addTopLevel("Evaluate", frame);
			    frame.setResizable(true);
			    rightDocked = false;
			} else {
			    rightDocked = true;
			}
		    }
		    if(leftDocked && t2Docked && rightDocked && t2Docked) {
			// no change
			return;
		    }
		    t1Docked = leftDocked;
		    t2Docked = rightDocked;
		    JSplitPane split = (JSplitPane)thisParent;
		    if(leftDocked) {
			if(rightDocked) {
			    finalSplit.setDividerLocation(0.5);
			} else {
			    finalSplit.setDividerLocation(1.0);
			}
			if(adjustVerticalSplit) {
			    split.setDividerLocation(0.66);
			}

		    } else if(rightDocked) {
			    finalSplit.setDividerLocation(0.0);
			    split.setDividerLocation(0.66);
		    } else {
			// both undocked
			split.setDividerLocation(1.0);
		    }
		}
		public void componentHidden(ComponentEvent e) {
		    check(e.getComponent());
		}
		public void componentMoved(ComponentEvent e) {
		    check(e.getComponent());
		}
		public void componentResized(ComponentEvent e) {
		    check(e.getComponent());
		}
		public void componentShown(ComponentEvent e) {
		    check(e.getComponent());
		}
	    };
	p1.addContainerListener(new ContainerListener() {
	    public void componentAdded(ContainerEvent e) {
		Component thisParent = finalThis.getParent();
		JSplitPane split = (JSplitPane)thisParent;
		if(e.getChild() == finalT1) {
		    if(finalT2.getParent() == finalP2) {
			// both docked
			finalSplit.setDividerLocation(0.5);
		    } else {
			// left docked only
			finalSplit.setDividerLocation(1.0);
		    }
		    split.setDividerLocation(0.66);
		}
	    }
	    public void componentRemoved(ContainerEvent e) {
		Component thisParent = finalThis.getParent();
		JSplitPane split = (JSplitPane)thisParent;
		if(e.getChild() == finalT1) {
		    if(finalT2.getParent() == finalP2) {
			// right docked only
			finalSplit.setDividerLocation(0.0);
			split.setDividerLocation(0.66);
		    } else {
			// both undocked
			split.setDividerLocation(1.0);
		    }
		}
	    }
	    });
	t1.addComponentListener(clistener);
	t2.addComponentListener(clistener);
	disable();
    }

    public void actionPerformed(ActionEvent e) {
	if(!enabled) return;
	if(e.getActionCommand().equals("ContextSwitch")) {
	    ContextHelper helper = new ContextHelper();
	    Context cx = db.getCurrentContext();
	    DebuggableEngine engine = cx.getDebuggableEngine();
	    helper.attach(cx);
	    int frameIndex = context.getSelectedIndex();
	    context.setToolTipText(toolTips.elementAt(frameIndex).toString());
	    Scriptable obj;
	    int frameCount = engine.getFrameCount();
	    if(frameIndex < frameCount) {
		obj = engine.getFrame(frameIndex).getVariableObject();
	    } else {
		helper.reset();
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
	    helper.reset();
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

    Main db;
    String url;
    String text;
    int line;
    boolean activate;

    CreateFileWindow(Main db,
		     String url, String text, int line) {
	this.db = db;
	this.url = url;
	this.text = text;
	this.line = line;
	this.activate = true;
    }

    CreateFileWindow(Main db,
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
	if(line != -1) {
	    if(db.currentWindow != null) {
		db.currentWindow.setPosition(-1);
	    }
	    try {
		w.setPosition(w.textArea.getLineStartOffset(line-1));
	    } catch(BadLocationException exc) {
		w.setPosition(-1);
	    }
	}
	db.desk.add(w);
	if(line != -1) {
	    db.currentWindow = w;
	}
	db.menubar.addFile(url);
	w.setVisible(true);
	if(activate) {
	    try {
		w.setMaximum(true);
		w.setSelected(true);
		w.moveToFront();
	    } catch(Exception exc) {
	    }
	}
    }
}

class SetFilePosition implements Runnable {

    Main db;
    FileWindow w;
    int line;
    boolean activate;

    SetFilePosition(Main db, FileWindow w, int line) {
	this.db = db; 
	this.w = w; 
	this.line = line; 
	activate = true;
    }

    SetFilePosition(Main db, FileWindow w, int line, boolean activate) {
	this.db = db; 
	this.w = w; 
	this.line = line; 
	this.activate = activate;
    }

    public void run() {
	JTextArea ta = w.textArea;
	try {
	    if(line == -1) {
		w.setPosition(-1);
		if(db.currentWindow == w) {
		    db.currentWindow = null;
		}
	    } else {
		int loc = ta.getLineStartOffset(line-1);
		if(db.currentWindow != null && db.currentWindow != w) {
		    db.currentWindow.setPosition(-1);
		}
		w.setPosition(loc);
		db.currentWindow = w;
	    }
	} catch(BadLocationException exc) {
	    // fix me
	} 
	if(activate) {
	    if(w.isIcon()) {
		db.desk.getDesktopManager().deiconifyFrame(w);
	    }
	    //db.desk.getDesktopManager().activateFrame(w);
	    try {
		w.show();
		//w.setSelected(true);
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
    Main db;
    Context cx;
    DebuggableEngine engine;
    UpdateContext(Main db, Context cx) {
	this.db = db;
	this.cx = cx;
	this.engine = cx.getDebuggableEngine();
    }

    public void run() {
	db.context.enable();
	JComboBox ctx = db.context.context;
	Vector toolTips = db.context.toolTips;
	db.context.disableUpdate();
	int frameCount = engine.getFrameCount();
	ctx.removeAllItems();
	toolTips.clear();
	for(int i = 0; i < frameCount; i++) {
	    DebugFrame frame = engine.getFrame(i);
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

    Menubar(Main db) {
	super();
	this.db = db;
	String[] fileItems  = {"Open...", "Run...", "", "Exit"};
	String[] fileCmds  = {"Open", "Load", "", "Exit"};
	char[] fileShortCuts = {'0', 'N', '\0', 'X'};
	int[] fileAccelerators = {KeyEvent.VK_O,
				  KeyEvent.VK_N,
				  0,
				  KeyEvent.VK_Q};
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
	    if(fileItems[i].length() == 0) {
		fileMenu.addSeparator();
	    } else {
		JMenuItem item = new JMenuItem(fileItems[i],
					       fileShortCuts[i]);
		item.setActionCommand(fileCmds[i]);
		item.addActionListener(this);
		fileMenu.add(item);
		if(fileAccelerators[i] != 0) {
		    KeyStroke k = KeyStroke.getKeyStroke(fileAccelerators[i], Event.CTRL_MASK);
		    item.setAccelerator(k);
		}
	    }
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
	breakOnExceptions = new JCheckBoxMenuItem("Break on Exceptions");
	breakOnExceptions.setMnemonic('X');
	breakOnExceptions.addActionListener(this);
	breakOnExceptions.setSelected(false);
	debugMenu.add(breakOnExceptions);
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
	} else if(cmd.equals("Break on Exceptions")) {
	    db.setBreakOnExceptions(breakOnExceptions.isSelected());
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
	JMenuItem lastItem = windowMenu.getItem(count -1);
	boolean hasMoreWin = false;
	int maxWin = 5;
	if(lastItem != null &&
	   lastItem.getText().equals("More Windows...")) {
	    hasMoreWin = true;
	    maxWin++;
	}
	if(!hasMoreWin && count - 4 == 5) {
	    windowMenu.add(item = new JMenuItem("More Windows...", 'M'));
	    item.setActionCommand("More Windows...");
	    item.addActionListener(this);
	    return;
	} else if(count - 4 <= maxWin) {
	    if(hasMoreWin) {
		count--;
		windowMenu.remove(lastItem);
	    }
	    File f = new File(fileName);
	    
	    windowMenu.add(item = new JMenuItem((char)('0' + (count-4)) + " " + f.getName(), '0' + (count - 4)));
	    if(hasMoreWin) {
		windowMenu.add(lastItem);
	    }
	} else {
	    return;
	}
	item.setActionCommand(fileName);
	item.addActionListener(this);
    }

    Main db;
    JMenu windowMenu;
    JCheckBoxMenuItem breakOnExceptions;
};

class EnterInterrupt implements Runnable {
    Main db;
    Context cx;
    EnterInterrupt(Main db, Context cx) {
	this.db = db;
	this.cx = cx;
    }
    public void run() {
	JMenu menu = db.getJMenuBar().getMenu(0); 
	//menu.getItem(0).setEnabled(false); // File->Load
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
	// raise the debugger window
	db.toFront();
    }
};

class ExitInterrupt implements Runnable {
    Main db;
    ExitInterrupt(Main db) {
	this.db = db;
    }
    public void run() {
	JMenu menu = db.getJMenuBar().getMenu(0); 
	menu.getItem(0).setEnabled(true); // File->Load
	menu = db.getJMenuBar().getMenu(2);
	menu.getItem(0).setEnabled(true); // Debug->Break
	int count = menu.getItemCount() - 1;
	int i = 1;
	for(; i < count; ++i) {
	    menu.getItem(i).setEnabled(false);
	}
	db.context.disable();
	boolean b = true;
	for(int ci = 0, cc = db.toolBar.getComponentCount(); ci < cc; ci++) {
	    db.toolBar.getComponent(ci).setEnabled(b);
	    b = false;
	}
	//db.console.consoleTextArea.requestFocus();
    }
};

class OpenFile implements Runnable {
    Scriptable scope;
    String fileName;
    Main db;
    OpenFile(Main db, Scriptable scope, String fileName) {
	this.scope = scope;
	this.fileName = fileName;
	this.db = db;
    }
    public void run() {
	Context cx = Context.enter();
	DebuggableEngine engine = cx.getDebuggableEngine();
	engine.setBreakNextLine(true);
	try {
	    cx.compileReader(scope, new FileReader(fileName),
			     fileName, 1, null);
	} catch(Exception exc) {
	    String msg = exc.getMessage();
	    if(exc instanceof EcmaError) {
		EcmaError err = (EcmaError)exc;
		msg = err.getSourceName() + ", line " + err.getLineNumber() + ": " + msg;
	    }
	    MessageDialogWrapper.showMessageDialog(db,
						   msg,
						   "Error Compiling File",
						   JOptionPane.ERROR_MESSAGE);
	} finally {
	    cx.exit();
	}
    }
}

class LoadFile implements Runnable {
    Scriptable scope;
    String fileName;
    Main db;
    LoadFile(Main db, Scriptable scope, String fileName) {
	this.scope = scope;
	this.fileName = fileName;
	this.db = db;
    }
    public void run() {
	Context cx = Context.enter();
	DebuggableEngine engine = cx.getDebuggableEngine();
	engine.setBreakNextLine(true);
	try {
	    cx.evaluateReader(scope, new FileReader(fileName),
			      fileName, 1, null);
	} catch(Exception exc) {
	    String msg = exc.getMessage();
	    if(exc instanceof EcmaError) {
		EcmaError err = (EcmaError)exc;
		msg = err.getSourceName() + ", line " + err.getLineNumber() + ": " + msg;
	    }
	    MessageDialogWrapper.showMessageDialog(db,
						   msg,
						   "Run",
						   JOptionPane.ERROR_MESSAGE);
	} finally {
	    cx.exit();
	}
    }
}


class ContextHelper {
    Context old;
    int enterCount;
    Context New;
    public void attach(Context cx) {
	old = Context.getCurrentContext();
	enterCount = 0;
	if(old != null) {
	    old.exit();
	    while(Context.getCurrentContext() != null) {
		enterCount++;
		old.exit();
	    }
	}
	Context.enter(cx);
	New = cx;
    }
    void reset() {
	New.exit();
	if(old != null) {
	    if(Context.enter(old) != old) {
		throw new RuntimeException("debugger error: failed to reset context");
	    }
	    while(enterCount > 0) {
		Context.enter();
		enterCount--;
	    }
	}
    }
}


public class Main extends JFrame implements Debugger, ContextListener {

    /* ContextListener interface */

    java.util.HashSet contexts = new java.util.HashSet();

    static Thread mainThread; // thread used to run the shell

    public void contextCreated(Context cx) {
	DebuggableEngine engine = cx.getDebuggableEngine();
	engine.setDebugger(this);
	cx.setGeneratingDebug(true);
	cx.setOptimizationLevel(-1);
	// if the user pressed "Break" or if this thread is the shell's 
	// Main then set the break flag so that when the debugger is run
	// with a file argument on the command line it will
	// break at the start of the file
	if(breakFlag || Thread.currentThread() == mainThread) {
	    engine.setBreakNextLine(true);
	}
    }
    
    public void contextEntered(Context cx) {
	// If the debugger is attached to cx
	// keep a reference to it even if it was detached
	// from its thread (we cause that to happen below
	// in interrupted)
	if(!contexts.contains(cx)) {
	    if(cx.getDebuggableEngine().getDebugger() == this) {
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

    boolean breakFlag = false;
    
    public void doBreak() {
	breakFlag = true;
	synchronized(contexts) {
	    Iterator iter = contexts.iterator();
	    while(iter.hasNext()) {
		Context cx = (Context)iter.next();
		cx.getDebuggableEngine().setBreakNextLine(true);
	    }
	}
    }

    public void setVisible(boolean b) {
	super.setVisible(b);
	if(b) {
	    // this needs to be done after the window is visible
	    console.consoleTextArea.requestFocus();
	    context.split.setDividerLocation(0.5);
	    try {
		console.setMaximum(true);
		console.setSelected(true);
		console.show();
		console.consoleTextArea.requestFocus();
	    } catch(Exception exc) {
	    }
	}
    }

    static final int STEP_OVER = 0;
    static final int STEP_INTO = 1;
    static final int STEP_OUT = 2;
    static final int GO = 3;
    static final int BREAK = 4;
    static final int RUN_TO_CURSOR = 5;
    static final int EXIT = 6;

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

    void doClearBreakpoints() {
	Enumeration e = breakpointsMap.keys();
	while(e.hasMoreElements()) {
	    String url = (String)e.nextElement();
	    Hashtable h =(Hashtable)breakpointsMap.get(url);
	    Enumeration he = h.keys();
	    while(he.hasMoreElements()) {
		Integer line = (Integer)h.get(he.nextElement());
		clearBreakPoint(url, line.intValue());
	    }
	}
    }


    /* Debugger Interface */
    
    public void handleCompilationDone(Context cx, DebuggableScript fnOrScript, 
                                      StringBuffer source) {
        String sourceName = fnOrScript.getSourceName();
        if (sourceName == null) {
	    sourceName = "<stdin>";
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
	breakFlag = false;
	interrupted(cx);
    }
    
    public void handleExceptionThrown(Context cx, Object e) {
	if(breakOnExceptions) {
	    DebuggableEngine engine = cx.getDebuggableEngine();
	    DebugFrame frame = engine.getFrame(0);  
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
		swingInvoke(new SetFilePosition(this, w, lineNumber));
	    }
	    MessageDialogWrapper.showMessageDialog(this,
						   msg,
						   "Exception in Script",
						   JOptionPane.ERROR_MESSAGE);
	    //if(w != null) {
	    //swingInvoke(new SetFilePosition(this, w, -1));
	    //}
	    interrupted(cx);
	}
    }

    /* end Debugger interface */
    
    JDesktopPane desk;
    ContextWindow context;
    Menubar menubar;
    JToolBar toolBar;
    JSInternalConsole console;
    EvalWindow evalWindow;
    JSplitPane split1;
    JLabel statusBar;

    void init() {
	setJMenuBar(menubar = new Menubar(this));
	toolBar = new JToolBar();
	JButton button;
	JButton breakButton, goButton, stepIntoButton,
	    stepOverButton, stepOutButton;
	button = breakButton = new JButton("Break");
	JButton focusButton = button;
	button.setToolTipText("Break");
	button.setActionCommand("Break");
	button.addActionListener(menubar);
	button.setEnabled(true);
	button = goButton = new JButton("Go");
	button.setToolTipText("Go");
	button.setActionCommand("Go");
	button.addActionListener(menubar);
	button.setEnabled(false);
	button = stepIntoButton = new JButton("Step Into");
	button.setToolTipText("Step Into");
	button.setActionCommand("Step Into");
	button.addActionListener(menubar);
	button.setEnabled(false);
	button = stepOverButton = new JButton("Step Over");
	button.setToolTipText("Step Over");
	button.setActionCommand("Step Over");
	button.setEnabled(false);
	button.addActionListener(menubar);
	button = stepOutButton = new JButton("Step Out");
	button.setToolTipText("Step Out");
	button.setActionCommand("Step Out");
	button.setEnabled(false);
	button.addActionListener(menubar);
	Dimension dim = stepOverButton.getPreferredSize();
	breakButton.setPreferredSize(dim);
	breakButton.setMinimumSize(dim);
	breakButton.setMaximumSize(dim);
	breakButton.setSize(dim);
	goButton.setPreferredSize(dim);
	goButton.setMinimumSize(dim);
	goButton.setMaximumSize(dim);
	stepIntoButton.setPreferredSize(dim);
	stepIntoButton.setMinimumSize(dim);
	stepIntoButton.setMaximumSize(dim);
	stepOverButton.setPreferredSize(dim);
	stepOverButton.setMinimumSize(dim);
	stepOverButton.setMaximumSize(dim);
	stepOutButton.setPreferredSize(dim);
	stepOutButton.setMinimumSize(dim);
	stepOutButton.setMaximumSize(dim);
	toolBar.add(breakButton);
	toolBar.add(goButton);
	toolBar.add(stepIntoButton);
	toolBar.add(stepOverButton);
	toolBar.add(stepOutButton);

	JPanel contentPane = new JPanel();
	contentPane.setLayout(new BorderLayout());
	getContentPane().add(toolBar, BorderLayout.NORTH);
	getContentPane().add(contentPane, BorderLayout.CENTER);
	desk = new JDesktopPane();
	desk.setPreferredSize(new Dimension(600, 300));
	desk.setMinimumSize(new Dimension(150, 50));
	desk.add(console = new JSInternalConsole("JavaScript Console"));
	context = new ContextWindow(this);
	context.setPreferredSize(new Dimension(600, 120));
	context.setMinimumSize(new Dimension(50, 50));

	split1 = new JSplitPane(JSplitPane.VERTICAL_SPLIT, desk,
					  context);
	split1.setOneTouchExpandable(true);
	Main.setResizeWeight(split1, 0.66);
	contentPane.add(split1, BorderLayout.CENTER);
	statusBar = new JLabel();
	statusBar.setText("Thread: ");
	contentPane.add(statusBar, BorderLayout.SOUTH);
	dlg = new JFileChooser();

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
	final Main self = this;
	addWindowListener(new WindowAdapter() {
		public void windowClosing(WindowEvent e) {
		    self.Exit();
		}
	    });
    }


    ScopeProvider scopeProvider;
    Runnable exitAction;


    Scriptable getScope() {
	return (scopeProvider != null) ? scopeProvider.getScope() : null;
    }

    FileWindow getFileWindow(String fileName) {
	if(fileName == null || fileName.equals("<stdin>") || fileName.equals("<eval>")) {
	    return null;
	}
	String file = fileName;
	Enumeration e = fileWindows.keys();
	for(; e.hasMoreElements(); ) {
	    String name = (String)e.nextElement();
	    if(file.equals(name)) {
		FileWindow w = (FileWindow)fileWindows.get(name);
		w.setUrl(fileName);
		return w;
	    }
	}
	return (FileWindow)fileWindows.get(fileName);
    }

    void loadedFile(String fileName, String text) {
	FileWindow w = getFileWindow(fileName);
	if(w != null) {
	    swingInvoke(new SetFileText(w, text));
	    w.show();
	} else if(!fileName.equals("<stdin>")) {
	    swingInvoke(new CreateFileWindow(this,
					     fileName,
					     text, 
					     -1));

	}
    }

    static void swingInvoke(Runnable f) {
	if(java.awt.EventQueue.isDispatchThread()) {
	    f.run();
	    return;
	}
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
	Context cx = getCurrentContext();
	DebuggableEngine engine = cx.getDebuggableEngine();
	ContextHelper helper = new ContextHelper();
	helper.attach(cx);
	if(cx != null) {
	    int frameCount = engine.getFrameCount();
	    if(frameIndex < 0 || frameIndex >= frameCount) {
		helper.reset();
		return;
	    }
	    this.frameIndex = frameIndex;
	    DebugFrame frame = engine.getFrame(frameIndex);
	    String sourceName = frame.getSourceName();
	    if(sourceName == null || sourceName.equals("<stdin>")) {
		console.show();
		helper.reset();
		return;
	    } 
	    if(sourceName == "<eval>") {
		helper.reset();
		return;
	    }
	    int lineNumber = frame.getLineNumber();
	    this.frameIndex = frameIndex;
	    FileWindow w = getFileWindow(sourceName);
	    if(w != null) {
		SetFilePosition action = 
		    new SetFilePosition(this, w, lineNumber);
		action.run();
	    } else {
		Vector v = (Vector)sourceNames.get(sourceName);
		String source = 
		    ((SourceEntry)v.elementAt(0)).source.toString();
		CreateFileWindow action = new CreateFileWindow(this, 
							       sourceName,
							       source,
							       lineNumber);
		action.run();
	    }
	    helper.reset();
	}
    }
    
    boolean isInterrupted = false;
    boolean nonDispatcherWaiting = false;
    int dispatcherIsWaiting = 0;
    Context currentContext = null;

    Context getCurrentContext() {
	return currentContext;
    }

    void interrupted(Context cx) {
	synchronized(swingMonitor) {
	    if(java.awt.EventQueue.isDispatchThread()) {
		dispatcherIsWaiting++;
		if(nonDispatcherWaiting) {
		    // Another thread is stopped in the debugger
		    // process events until it resumes and we
		    // can enter
		    java.awt.EventQueue eventQ  = 
			java.awt.Toolkit.getDefaultToolkit().getSystemEventQueue();
		    while(nonDispatcherWaiting) {
			try {
			    AWTEvent event = eventQ.getNextEvent();
			    if(event instanceof ActiveEvent) {
				((ActiveEvent)event).dispatch();
			    } else {
				Object source = event.getSource();
				if(source instanceof Component) {
				    Component comp = (Component)source;
				// Suppress Window/InputEvent's that aren't
				// directed to the Debugger
				    // if(!(event instanceof InputEvent || 
				    //event instanceof WindowEvent)||
				    //shouldDispatchTo(comp)) {
				    comp.dispatchEvent(event);
				//}
				} else if(source instanceof MenuComponent) {
				    ((MenuComponent)source).dispatchEvent(event);
				} 
			    }
			    if(this.returnValue == EXIT) {
				return;
			    }
			    swingMonitor.wait(1);
			} catch(InterruptedException exc) {
			    return;
			}
		    }
		}
	    } else {
		while(isInterrupted || dispatcherIsWaiting > 0) {
		    try {
			swingMonitor.wait();
		    } catch(InterruptedException exc) {
			return;
		    }
		}
		nonDispatcherWaiting = true;
	    }
	    isInterrupted = true;
	}
	do {
	    currentContext = cx;
	    DebuggableEngine engine = cx.getDebuggableEngine();
	    Thread thread = Thread.currentThread();
	    statusBar.setText("Thread: " + thread.toString());
	    ThreadState state = (ThreadState)threadState.get(thread);
	    int stopAtFrameDepth = -1;
	    if(state != null) {
		stopAtFrameDepth = state.stopAtFrameDepth;
	    }
	    if(runToCursorFile != null && thread == runToCursorThread) {
		int frameCount = engine.getFrameCount();
		if(frameCount > 0) {
		    DebugFrame frame = engine.getFrame(0);  
		    String sourceName = frame.getSourceName();
		    if(sourceName != null) {
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
		if (engine.getFrameCount() > stopAtFrameDepth) {
		    break;
		}
	    }
	    if(state != null) {
		state.stopAtFrameDepth = -1;
	    }
	    threadState.remove(thread);
	    int frameCount = engine.getFrameCount();
	    this.frameIndex = frameCount -1;
	    int line = 0;
	    if(frameCount == 0) {
		break;
	    }
	    DebugFrame frame = engine.getFrame(0);
	    String fileName = frame.getSourceName();
	    engine.setBreakNextLine(false);
	    line = frame.getLineNumber();
	    int enterCount = 0;
	    boolean isDispatchThread = 
		java.awt.EventQueue.isDispatchThread();

	    if(!isDispatchThread) {
		// detach cx from its thread so the debugger (in the awt 
		// dispatcher thread) can enter it if necessary
		cx.exit();
		while(Context.getCurrentContext() != null) {
		    Context.exit();
		    enterCount++;
		}
	    }
	    if(fileName != null && !fileName.equals("<stdin>")) {
		FileWindow w = (FileWindow)getFileWindow(fileName);
		if(w != null) {
		    SetFilePosition action = 
			new SetFilePosition(this, w, line);
		    swingInvoke(action);
		} else {
		    Vector v = (Vector)sourceNames.get(fileName);
		    String source = 
			((SourceEntry)v.elementAt(0)).source.toString();
		    CreateFileWindow action = new CreateFileWindow(this, 
								   fileName,
								   source,
								   line);
		    swingInvoke(action);
		}
	    } else {
		if(console.isVisible()) {
		    final JSInternalConsole finalConsole = console;
		    swingInvoke(new Runnable() {
			    public void run() {
				finalConsole.show();
			    }
			});
		}
	    }
	    swingInvoke(new EnterInterrupt(this, cx));
	    swingInvoke(new UpdateContext(this, cx));
	    int returnValue;
	    if(!isDispatchThread) {
		synchronized(monitor) {
		    this.returnValue = -1;
		    try {
			while(this.returnValue == -1) {
			    monitor.wait();
			}
			returnValue = this.returnValue;
		    } catch(InterruptedException exc) {
			break;
		    }
		}
	    } else {
		java.awt.EventQueue eventQ  = 
		    java.awt.Toolkit.getDefaultToolkit().getSystemEventQueue();
		this.returnValue = -1;
		while(this.returnValue == -1) {
		    try {
			AWTEvent event = eventQ.getNextEvent();
			if(event instanceof ActiveEvent) {
			    ((ActiveEvent)event).dispatch();
			} else {
			    Object source = event.getSource();
			    if(source instanceof Component) {
				Component comp = (Component)source;
				// Suppress Window/InputEvent's that aren't
				// directed to the Debugger
				// if(!(event instanceof InputEvent || 
				//event instanceof WindowEvent)||
				//       shouldDispatchTo(comp)) {
				//comp.dispatchEvent(event);
				//}
				comp.dispatchEvent(event);
			    } else if(source instanceof MenuComponent) {
				((MenuComponent)source).dispatchEvent(event);
			    } 
			}
		    } catch(InterruptedException exc) {
		    }
		}
		returnValue = this.returnValue;
	    }
	    swingInvoke(new ExitInterrupt(this));
	    if(!isDispatchThread) {
		// reattach cx to its thread
		Context current;
		if((current = Context.enter(cx)) != cx) {
		    System.out.println("debugger error: cx = " + cx + " current = " + current);
		    
		}
		while(enterCount > 0) {
		    Context.enter();
		    enterCount--;
		}
	    }
	    switch(returnValue) {
	    case STEP_OVER:
		engine.setBreakNextLine(true);
		stopAtFrameDepth = engine.getFrameCount();
		if(state == null) {
		    state = new ThreadState();
		}
		state.stopAtFrameDepth = stopAtFrameDepth;
		threadState.put(thread, state);
		break;
	    case STEP_INTO:
		engine.setBreakNextLine(true);
		if(state != null) {
		    state.stopAtFrameDepth = -1;
		}
		break;
	    case STEP_OUT:
		stopAtFrameDepth = engine.getFrameCount() -1;
		if(stopAtFrameDepth > 0) {
		    engine.setBreakNextLine(true);
		    if(state == null) {
			state = new ThreadState();
		    }
		    state.stopAtFrameDepth = stopAtFrameDepth;
		    threadState.put(thread, state);
		}
		break;
	    case RUN_TO_CURSOR:
		engine.setBreakNextLine(true);
		if(state != null) {
		    state.stopAtFrameDepth = -1;
		}
		break;
	    }
	} while(false);
	synchronized(swingMonitor) {
	    isInterrupted = false;
	    if(java.awt.EventQueue.isDispatchThread()) {
		dispatcherIsWaiting--;
	    } else {
		nonDispatcherWaiting = false;
	    }
	    swingMonitor.notifyAll();
	}
    }

    JFileChooser dlg;

    String chooseFile(String title) {
	dlg.setDialogTitle(title);
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
            try {
                String result = dlg.getSelectedFile().getCanonicalPath();
                CWD = dlg.getSelectedFile().getParentFile();
                java.lang.System.setProperty("user.dir", CWD.getPath());
                return result;
            } catch(IOException ignored) {
            }
	}
	return null;
    }

    JInternalFrame getSelectedFrame() {
       JInternalFrame[] frames = desk.getAllFrames();
       for(int i = 0; i < frames.length; i++) {
           if(frames[i].isShowing()) {
               return frames[i];
           }
       }
       return frames[frames.length - 1];
    }

    void actionPerformed(ActionEvent e) {
	String cmd = e.getActionCommand();
	int returnValue = -1;
	if(cmd.equals("Cut") || cmd.equals("Copy") || cmd.equals("Paste")) {
            JInternalFrame f = getSelectedFrame();
            if(f != null && f instanceof ActionListener) {
                ((ActionListener)f).actionPerformed(e);
            }
	} else if(cmd.equals("Step Over")) {
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
	    Exit();
	} else if(cmd.equals("Open")) {
	    Scriptable scope = getScope();
	    if(scope == null) {
		MessageDialogWrapper.showMessageDialog(this, "Can't compile scripts: no scope available", "Open", JOptionPane.ERROR_MESSAGE);
	    } else {
		String fileName = chooseFile("Select a file to compile");
		if(fileName != null) {
		    new Thread(new OpenFile(this, scope,
					    fileName)).start();
		}
	    }
	} else if(cmd.equals("Load")) {
	    Scriptable scope = getScope();
	    if(scope == null) {
		MessageDialogWrapper.showMessageDialog(this, "Can't run scripts: no scope available", "Run", JOptionPane.ERROR_MESSAGE);
	    } else {
		String fileName = chooseFile("Select a file to execute");
		if(fileName != null) {
		    new Thread(new LoadFile(this, scope,
					    fileName)).start();
		}
	    }
	} else if(cmd.equals("More Windows...")) {
	    MoreWindows dlg = new MoreWindows(this, fileWindows, 
					      "Window", "Files");
	    dlg.showDialog(this);
	} else if(cmd.equals("Console")) {
	    if(console.isIcon()) {
		desk.getDesktopManager().deiconifyFrame(console);
	    }
	    console.show();
	    desk.getDesktopManager().activateFrame(console);
	    console.consoleTextArea.requestFocus();
	} else if(cmd.equals("Cut")) {
	} else if(cmd.equals("Copy")) {
	} else if(cmd.equals("Paste")) {
	} else if(cmd.equals("Go to function...")) {
	    FindFunction dlg = new FindFunction(this, functionMap, 
						"Go to function", 
						"Function");
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
		    desk.getDesktopManager().setBoundsForFrame(f, x, y, 
							       w, h);
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
		try {
		    if(w.isIcon()) {
			w.setIcon(false);
		    }
		    w.setVisible(true);
		    w.moveToFront();
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

    void removeWindow(FileWindow w) {
	fileWindows.remove(w.getUrl());
	JMenu windowMenu = getWindowMenu();
	int count = windowMenu.getItemCount();
	JMenuItem lastItem = windowMenu.getItem(count -1);
	String name = new File(w.getUrl()).getName();
	for(int i = 5; i < count; i++) {
	    JMenuItem item = windowMenu.getItem(i);
	    if(item == null) continue; // separator
	    String text = item.getText();
	    //1 D:\foo.js
	    //2 D:\bar.js
	    int pos = text.indexOf(' ');
	    if(text.substring(pos + 1).equals(name)) {
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
		    for(;i < count -1; i++) {
			JMenuItem thisItem = windowMenu.getItem(i);
			if(thisItem != null) {
			    //1 D:\foo.js
			    //2 D:\bar.js
			    text = thisItem.getText();
			    if(text.equals("More Windows...")) {
				break;
			    } else {
				pos = text.indexOf(' ');
				thisItem.setText((char)('0' + j) + " " +
						 text.substring(pos + 1));
				thisItem.setMnemonic('0' + j);
				j++;
			    }
			}
		    }
		    if(count - 6 == 0 && lastItem != item) {
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
    
    boolean stringIsCompilableUnit(String expr) {
	Context cx = Context.enter();
	boolean result = cx.stringIsCompilableUnit(expr);
	cx.exit();
	return result;
    }

    String eval(String expr) {
	Context cx = getCurrentContext();
	DebuggableEngine engine = cx.getDebuggableEngine();
	if(cx == null) return "undefined";
	ContextHelper helper = new ContextHelper();
	helper.attach(cx);
	if(frameIndex >= engine.getFrameCount()) {
	    helper.reset();
	    return "undefined";
	}
	String resultString;
	engine.setDebugger(null);
	cx.setGeneratingDebug(false);
	cx.setOptimizationLevel(-1);
	boolean breakNextLine = engine.getBreakNextLine();
	engine.setBreakNextLine(false);
	try {
	    Scriptable scope;
	    scope = engine.getFrame(frameIndex).getVariableObject();
	    Object result;
	    if(scope instanceof NativeCall) {
		NativeCall call = (NativeCall)scope;
		result = NativeGlobal.evalSpecial(cx, call,
						  call.getThisObj(),
						  new Object[]{expr},
						  "", 1);
	    } else {
		result = cx.evaluateString(scope, expr, "", 0, null);
	    }
	    if(result == Undefined.instance) {
		result = "";
	    }
	    try {
		resultString = ScriptRuntime.toString(result);
	    } catch(Exception exc) {
		resultString = result.toString();
	    }
	} catch(Exception exc) {
	    resultString = exc.getMessage();
	}
	if(resultString == null) {
	    resultString = "null";
	}
	engine.setDebugger(this);
	cx.setGeneratingDebug(true);
	cx.setOptimizationLevel(-1);
	engine.setBreakNextLine(breakNextLine);
	helper.reset();
	return resultString;
    }

    void Exit() {
	// stop handling events
	this.returnValue = EXIT;
	// call the exit handler if any
	if(exitAction != null) {
	    swingInvokeLater(exitAction);
	}
    }
    
    java.util.Hashtable fileWindows = new java.util.Hashtable();
    FileWindow currentWindow;
    Object monitor = new Object();
    Object swingMonitor = new Object();
    int returnValue = -1;
    boolean breakOnExceptions;

    static void setResizeWeight(JSplitPane pane, double weight) {
	// call through reflection for portability
	// pre-1.3 JDK JSplitPane doesn't have this method
	try {
	    Method m = JSplitPane.class.getMethod("setResizeWeight",
						  new Class[]{double.class});
	    m.invoke(pane, new Object[]{new Double(weight)});
	} catch(NoSuchMethodException exc) {
	} catch(IllegalAccessException exc) {
	} catch(java.lang.reflect.InvocationTargetException exc) {
	}
    }

    java.util.Hashtable toplevels = new java.util.Hashtable();

    boolean shouldDispatchTo(Component source) {
	Component root = SwingUtilities.getRoot(source);
	if(root == this) {
	    return true;
	}
	Enumeration e = toplevels.keys();
	while(e.hasMoreElements()) {
	    Object key = e.nextElement();
	    JFrame frame = (JFrame)toplevels.get(key);
	    if(root == frame) {
		return true;
	    }
	}
	return false;
    }

    void addTopLevel(String key, JFrame frame) {
	if(frame != this) {
	    toplevels.put(key, frame);
	}
    }

    void setBreakOnExceptions(boolean value) {
	this.breakOnExceptions = value;
    }


    //
    // public interface
    //

    public Main(String title) {
	super(title);
	init();
    }

    /**
     * 
     * Remove all breakpoints
     */
    public void clearAllBreakpoints() {
	doClearBreakpoints();
    }

    /**
     * Assign an object that can provide a Scriptable that will
     * be used as the scope for loading scripts from files
     * when the user selects "Open..." or "Run..."
     */

    public void setScopeProvider(ScopeProvider p) {
	scopeProvider = p;
    }

    /**
     * Assign a Runnable object that will be invoked when the user
     * selects "Exit..." or closes the Debugger main window 
     */
    
    public void setExitAction(Runnable r) {
	exitAction = r;
    }

    /**
     * Get an input stream to the Debugger's internal Console window
     */

    public InputStream getIn() {
	return console.getIn();
    }

    /**
     * Get an output stream to the Debugger's internal Console window
     */

    public PrintStream getOut() {
	return console.getOut();
    }

    /**
     * Get an error stream to the Debugger's internal Console window
     */

    public PrintStream getErr() {
	return console.getErr();
    }

    public static void main(String[] args) {
	try {
	    mainThread = Thread.currentThread();
	    final Main sdb = new Main("Rhino JavaScript Debugger");
	    swingInvoke(new Runnable() {
		    public void run() {
			sdb.pack();
			sdb.setSize(600, 460);
			sdb.setVisible(true);
		    }
		});
	    sdb.setExitAction(new Runnable() {
		    public void run() {
			System.exit(0);
		    }
		});
	    System.setIn(sdb.getIn());
	    System.setOut(sdb.getOut());
	    System.setErr(sdb.getErr());
	    Context.addContextListener(sdb);
	    sdb.setScopeProvider(new ScopeProvider() {
		    public Scriptable getScope() {
			return org.mozilla.javascript.tools.shell.Main.getScope();
		    }
		});
	    org.mozilla.javascript.tools.shell.Main.exec(args);
	} catch(Exception exc) {
	    exc.printStackTrace();
	}
    }

};

