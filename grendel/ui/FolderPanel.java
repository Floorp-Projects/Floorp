/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Will Scullin <scullin@netscape.com>,  3 Sep 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import java.awt.Cursor;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.Toolkit;
import java.awt.datatransfer.Clipboard;
import java.awt.datatransfer.ClipboardOwner;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.awt.event.ActionEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseEvent;
import java.io.IOException;
import java.util.Enumeration;
import java.util.Locale;
import java.util.ResourceBundle;
import java.util.Vector;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Flags;

import javax.swing.BorderFactory;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.JPopupMenu;
import javax.swing.JScrollPane;
//import javax.swing.JToolBar;
import javax.swing.KeyStroke;
import javax.swing.ToolTipManager;
import javax.swing.event.ChangeEvent;
//import javax.swing.plaf.BorderUIResource;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

//import netscape.orion.toolbars.NSToolbar;
//import netscape.orion.uimanager.AbstractUICmd;
//import netscape.orion.uimanager.IUICmd;

import grendel.composition.Composition;
import grendel.storage.MessageExtra;
import grendel.storage.MessageExtraFactory;
import grendel.ui.UIAction;
import grendel.view.FolderView;
import grendel.view.FolderViewFactory;
import grendel.view.MessageSetView;
import grendel.view.MessageSetViewObserver;
import grendel.view.ViewedFolder;
import grendel.view.ViewedMessage;
import grendel.view.ViewedStore;
import grendel.widgets.CellRenderer;
import grendel.widgets.Column;
import grendel.widgets.ColumnHeader;
import grendel.widgets.ColumnModel;
import grendel.widgets.ColumnModelEvent;
import grendel.widgets.ColumnModelListener;
import grendel.widgets.DefaultCellRenderer;
import grendel.widgets.GrendelToolBar;
import grendel.widgets.SelectionEvent;
import grendel.widgets.SelectionListener;
import grendel.widgets.SelectionManager;
import grendel.widgets.StatusEvent;
import grendel.widgets.ToggleCellEditor;
import grendel.widgets.ToggleCellRenderer;
import grendel.widgets.TreePath;
import grendel.widgets.TreeTable;
import grendel.widgets.TreeTableDataModel;

import calypso.util.Assert;

/**
 * Panel to display the <em>contents</em> of a folder.
 */


public class FolderPanel extends GeneralPanel {

  //
  // Private members
  //

  TreeTable           fMessageTree;
  Folder              fFolder = null;
  Thread              fFolderLoadThread = null;
  FolderView          fView = null;
  MessageModel        fModel = null;
  FolderPanel         fPanel;
  FolderPanelListener fListeners = null;
  SelectionListener   fSelectionListener = null;
  ColumnModel         fColumnModel = null;

  /**
   * The Sender column ID
   */

  public static final String  kSenderID = new String("sender");

  /**
   * The Subject column ID
   */

  public static final String  kSubjectID = new String("subject");

  /**
   * The Date column ID
   */

  public static final String  kDateID = new String("date");

  /**
   * The read column ID
   */

  public static final String  kReadID = new String("read");

  /**
   * The flag column ID
   */

  public static final String  kFlagID = new String("flag");

  /**
   * The Deleted (X) column ID
   */

  public static final String  kDeletedID = new String("deleted");

  /**
   * The individual message scope
   */

  public static final int     kMessage = 0;

  /**
   * The thread scope
   */

  public static final int     kThread = 1;

  /**
   * The all messages scope
   */

  public static final int     kAll = 2;

  /**
   * Forward Quoted
   */

  public static final int     kQuoted = 0;

  /**
   * Forward Inline
   */

  public static final int     kInline = 1;

  /**
   * Forward as Attachment
   */

  public static final int     kAttachment = 2;

  //
  // Actions that can be enabled/disabled
  //

  DeleteMessageAction fDeleteMessageAction = new DeleteMessageAction();
  OpenMessageAction fOpenMessageAction = new OpenMessageAction();

  ReplyAction fReplyAction = new ReplyAction("msgReply", false);
  ReplyAction fReplyAllAction = new ReplyAction("msgReplyAll", true);

  ForwardAction fForwardQuoted = new ForwardAction("fwdQuoted", kQuoted);
  ForwardAction fForwardInline = new ForwardAction("fwdInline", kInline);
  ForwardAction fForwardAttachment = new ForwardAction("fwdAttachment", kAttachment);

  MarkAction fMarkMsgReadAction = new MarkAction("markMsgRead", kMessage);
  MarkAction fMarkThreadReadAction = new MarkAction("markThreadRead", kThread);
  MarkAction fMarkAllReadAction = new MarkAction("markAllRead", kAll);

  ThreadAction fThreadAction = new ThreadAction();
  // The big action list

  UIAction            fActions[] = {ActionFactory.GetNewMailAction(),
                                    ActionFactory.GetComposeMessageAction(),
                                    fDeleteMessageAction,
                                    fOpenMessageAction,
                                    new CopyToClipboardAction(),
                                    new PasteFromClipboardAction(),
                                    fThreadAction,
                                    fReplyAction,
                                    fReplyAllAction,
                                    fForwardQuoted,
                                    fForwardInline,
                                    fForwardAttachment,
                                    fMarkMsgReadAction,
                                    fMarkThreadReadAction,
                                    fMarkAllReadAction
  };

  SortAction      fSortActions[] = {new SortAction("sortAuthor",
                                                   MessageSetView.AUTHOR),
                                    new SortAction("sortSubject",
                                                   MessageSetView.SUBJECT),
                                    new SortAction("sortDate",
                                                   MessageSetView.DATE),
                                    new SortAction("sortNumber",
                                                   MessageSetView.NUMBER)
  };

  /**
   * Constructs a folder panel
   */

  public FolderPanel() {
    JScrollPane scrollPane = new JScrollPane();
    //scrollPane.setBorder(BorderUIResource.getLoweredBevelBorderUIResource());
    scrollPane.setBorder(BorderFactory.createLoweredBevelBorder());
    Util.RegisterScrollingKeys(scrollPane);

    fPanel = this;

    fModel = new MessageModel();

    fMessageTree = new TreeTable(null);
    ToolTipManager.sharedInstance().registerComponent(fMessageTree);
    fMessageTree.setTreeColumn(kSubjectID);

    fColumnModel = fMessageTree.getColumnModel();
    fColumnModel.addColumnModelListener(new ColumnListener());

    Column column;
    CellRenderer cellrenderer = new MessageCellRenderer();

    column = new Column(kSubjectID, fLabels.getString("subjectLabel"));
    column.setWidth(350);
    column.setSelectable(true);
    column.setCellRenderer(cellrenderer);
    fMessageTree.addColumn(column);

    ToggleCellRenderer renderer;
    ToggleCellEditor editor;

    Icon unreadIcon = new ImageIcon(getClass().getResource("images/unread.gif"));
    Icon readIcon = new ImageIcon(getClass().getResource("images/read.gif"));

    renderer = new ToggleCellRenderer();
    renderer.getCheckBox().setIcon(unreadIcon);
    renderer.getCheckBox().setSelectedIcon(readIcon);
    editor = new ToggleCellEditor();
    editor.getCheckBox().setIcon(unreadIcon);
    editor.getCheckBox().setSelectedIcon(readIcon);

    column = new Column(kReadID, unreadIcon);
    column.setWidth(20);
    column.setMinWidth(20);
    column.setMaxWidth(20);
    column.setSelectable(true);
    column.setCellRenderer(renderer);
    column.setCellEditor(editor);
    fMessageTree.addColumn(column);

    Icon unflaggedIcon = new ImageIcon(getClass().getResource("images/unflagged.gif"));
    Icon flaggedIcon = new ImageIcon(getClass().getResource("images/flagged.gif"));

    renderer = new ToggleCellRenderer();
    renderer.getCheckBox().setIcon(unflaggedIcon);
    renderer.getCheckBox().setSelectedIcon(flaggedIcon);
    editor = new ToggleCellEditor();
    editor.getCheckBox().setIcon(unflaggedIcon);
    editor.getCheckBox().setSelectedIcon(flaggedIcon);

    column = new Column(kFlagID, flaggedIcon);
    column.setWidth(20);
    column.setMinWidth(20);
    column.setMaxWidth(20);
    column.setSelectable(true);
    column.setCellRenderer(renderer);
    column.setCellEditor(editor);
    fMessageTree.addColumn(column);

    Icon deletedIcon = new ImageIcon(getClass().getResource("images/deleted.gif"));
    Icon undeletedIcon = new ImageIcon(getClass().getResource("images/unflagged.gif"));

    renderer = new ToggleCellRenderer();
    renderer.getCheckBox().setIcon(undeletedIcon);
    renderer.getCheckBox().setSelectedIcon(deletedIcon);
    editor = new ToggleCellEditor();
    editor.getCheckBox().setIcon(undeletedIcon);
    editor.getCheckBox().setSelectedIcon(deletedIcon);

    column = new Column(kDeletedID, deletedIcon);
    column.setWidth(20);
    column.setMinWidth(20);
    column.setMaxWidth(20);
    column.setSelectable(true);
    column.setCellRenderer(renderer);
    column.setCellEditor(editor);
    fMessageTree.addColumn(column);

    column = new Column(kSenderID, fLabels.getString("senderLabel"));
    column.setWidth(250);
    column.setSelectable(true);
    column.setCellRenderer(cellrenderer);
    fMessageTree.addColumn(column);

    column = new Column(kDateID, fLabels.getString("dateLabel"));
    column.setWidth(100);
    column.setSelectable(true);
    column.setCellRenderer(cellrenderer);
    fMessageTree.addColumn(column);

    Preferences prefs = PreferencesFactory.Get();
    fColumnModel.setPrefString(prefs.getString("mail.folder_panel.column_layout", ""));

    // Setup keys
    registerKeyboardAction(new DeleteMessageAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_DELETE, 0),
                           WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);
    registerKeyboardAction(new CopyToClipboardAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_C,
                                                  KeyEvent.CTRL_MASK),
                           WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);
    registerKeyboardAction(new PasteFromClipboardAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_V,
                                                  KeyEvent.CTRL_MASK),
                           WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);
    registerKeyboardAction(new OpenMessageAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_O,
                                                  KeyEvent.CTRL_MASK),
                           WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    ColumnHeader columnHeader = fMessageTree.getColumnHeader();
    scrollPane.setColumnHeaderView(columnHeader);
    scrollPane.setViewportView(fMessageTree);

    add(scrollPane);

    fSelectionListener = new MessageSelectionListener();
    fMessageTree.getSelectionManager().addSelectionListener(fSelectionListener);
  }

  public void dispose() {
    setFolder(null);

    Preferences prefs = PreferencesFactory.Get();
    prefs.putString("mail.folder_panel.column_layout",
                    fColumnModel.getPrefString());

    fMessageTree.getSelectionManager().removeSelectionListener(fSelectionListener);
  }

  /**
   * Sets the folder displayed to the given folder. If the folder is
   * null, the folder display is cleared.
   */

  public synchronized void setFolder(Folder aFolder) {
    if (fFolderLoadThread != null) {
      fFolderLoadThread.stop();
      if (fListeners != null) {
        fListeners.folderLoaded(new ChangeEvent(fPanel));
        fListeners.folderStatus(new StatusEvent(fPanel,
                                    fLabels.getString("folderLoadedStatus")));
      }
    }
    fFolderLoadThread =
      new Thread(new LoadFolderThread(aFolder), "folderLoad");
    int maxPriority = fFolderLoadThread.getThreadGroup().getMaxPriority();
    fFolderLoadThread.setPriority(maxPriority);
    fFolderLoadThread.start();
  }

  /**
   * Returns the current folder
   */

  public Folder getFolder() {
    return fFolder;
  }

  /**
   * Returns an enumeration of <code>TreePaths<code>s representing
   * the current selection. The tips of the paths are ViewedMessages.
   */

  public Enumeration getSelection() {
    return fMessageTree.getSelectionManager().getSelection();
  }

  /**
   * Returns the actions available for this panel
   */

  public UIAction[] getActions() {
    return Util.MergeActions(fActions, fSortActions);
  }

  /**
   * Returns the toolbar associated with this panel.
   */

  public GrendelToolBar getToolBar() {
    return buildToolBar("folderToolBar", getActions());
  }

  /**
   * Adds a FolderPanelListener
   */

  public void addFolderPanelListener(FolderPanelListener aListener) {
    fListeners = FolderMulticaster.add(fListeners, aListener);
  }

  /**
   * Removes a FolderPanelListener
   */

  public void removeFolderPanelListener(FolderPanelListener aListener) {
    fListeners = FolderMulticaster.remove(fListeners, aListener);
  }


  //
  // Component overloads
  //

  public boolean isOpaque() {
    return true;
  }

  /**
   * Syncs the sort menu items when the view or sort changes
   */

  void updateSortMenu() {
    if (fView != null) {
      int order[] = fView.getSortOrder();
      if (order != null && order.length > 0) {
        for (int i = 0; i < fSortActions.length; i++) {
          if (fSortActions[i].getType() == order[0]) {
            fSortActions[i].setSelected(true);
          }
        }
      }
      fThreadAction.setSelected(fView.isThreaded() ?
                                true : false);
    }
  }

  //
  // MessageSelectionListener class
  //

  class MessageSelectionListener implements SelectionListener {
    public void selectionChanged(SelectionEvent aEvent) {
      if (fListeners != null) {
        fListeners.folderSelectionChanged(new ChangeEvent(fPanel));
      }

      // Interpret selection
      SelectionManager sm = (SelectionManager) aEvent.getSource();
      boolean one = sm.getSelectionCount() == 1;
      boolean many = sm.getSelectionCount() > 0;

      fDeleteMessageAction.setEnabled(many);
      fOpenMessageAction.setEnabled(many);

      fReplyAction.setEnabled(one);
      fReplyAllAction.setEnabled(one);

      fMarkMsgReadAction.setEnabled(many);
      fMarkThreadReadAction.setEnabled(many);
    }

    public void selectionDoubleClicked(MouseEvent aEvent) {
      if (fListeners != null) {
        fListeners.folderSelectionDoubleClicked(new ChangeEvent(fPanel));
      }
    }

    protected void buildMenus(ViewedFolder aFolder, JMenu aCopy, JMenu aMove) {
      int goodMask = Folder.HOLDS_MESSAGES|Folder.READ_WRITE;
      if (aFolder != null) {
        Folder folder = aFolder.getFolder();
        ViewedFolder temp = aFolder.getFirstSubFolder();

        if (temp == null) {
          try {
            if ((aFolder.getFolder().getType() & goodMask) == goodMask) {
              JMenuItem copyItem = new JMenuItem(folder.getName());
              JMenuItem moveItem = new JMenuItem(folder.getName());

              copyItem.addActionListener(new CopyMessageAction(folder));
              moveItem.addActionListener(new MoveMessageAction(folder));

              aCopy.add(copyItem);
              aMove.add(moveItem);
            }
          } catch (MessagingException e) {}
        }  else {
          JMenu copyMenu = new JMenu(folder.getName());
          JMenu moveMenu = new JMenu(folder.getName());

          while (temp != null) {
            buildMenus(temp, copyMenu, moveMenu);
            temp = temp.getNextFolder();
          }

          if (copyMenu.getMenuComponentCount() > 0) {
            aCopy.add(copyMenu);
            aMove.add(moveMenu);
          }
        }
      }
    }

    /**
     * Attempt to bring up a context menu
     */

    public void selectionContextClicked(MouseEvent aEvent) {
      JPopupMenu popup = new JPopupMenu();
      JMenuItem item;

      ResourceBundle strings = ResourceBundle.getBundle("grendel.ui.MenuLabels",
                                                        getLocale());

      item = new JMenuItem(strings.getString("msgDeletePopupLabel"));
      item.addActionListener(new DeleteMessageAction());
      popup.add(item);

      JMenu copyMenu = new JMenu(strings.getString("msgCopyPopupLabel"));
      JMenu moveMenu = new JMenu(strings.getString("msgMovePopupLabel"));

      ViewedStore stores[] = StoreFactory.Instance().getStores();

      for (int i = 0; i < stores.length; i++) {
        buildMenus(stores[i], copyMenu, moveMenu);
      }

      popup.add(copyMenu);
      popup.add(moveMenu);

      popup.show(fPanel, aEvent.getX(), aEvent.getY());
    }

    public void selectionDragged(MouseEvent aEvent) {
      System.out.println("Dragging started");
    }
  }

  //
  // ColumnModelListener
  //

  class ColumnListener implements ColumnModelListener {
    public void columnAdded(ColumnModelEvent e) {}
    public void columnRemoved(ColumnModelEvent e) {}
    public void columnMoved(ColumnModelEvent e) {}
    public void columnMarginChanged(ChangeEvent e) {}
    public void columnWidthChanged(ColumnModelEvent e) {}

    public void columnSelectionChanged(ChangeEvent e) {
      Column column = fColumnModel.getSelectedColumn();
      if (fView != null && column != null) {
        Object id = column.getID();
        if (id == kSenderID) {
          fView.prependSortOrder(MessageSetView.AUTHOR);
        } else if (id == kSubjectID) {
          fView.prependSortOrder(MessageSetView.SUBJECT);
        } else if (id == kDateID) {
          fView.prependSortOrder(MessageSetView.DATE);
        } else if (id == kReadID) {
          fView.prependSortOrder(MessageSetView.READ);
        } else if (id == kFlagID) {
          fView.prependSortOrder(MessageSetView.FLAGGED);
        } else if (id == kDeletedID) {
          fView.prependSortOrder(MessageSetView.DELETED);
        }
        fView.reThread();
        updateSortMenu();
      }
    }
  }

  /**
   * Returns the current selection as a vector
   */

  synchronized Vector getSelectionVector() {
    Vector msgVector = new Vector();
    SelectionManager selection = fMessageTree.getSelectionManager();
    Enumeration messages = selection.getSelection();
    while (messages.hasMoreElements()) {
      msgVector.insertElementAt(messages.nextElement(), msgVector.size());
    }
    return msgVector;
  }

  synchronized Vector getSelectedMessageVector() {
    Vector msgVector = new Vector();
    SelectionManager selection = fMessageTree.getSelectionManager();
    Enumeration messages = selection.getSelection();
    while (messages.hasMoreElements()) {
      TreePath path = (TreePath) messages.nextElement();
      Message msg = ((ViewedMessage) path.getTip()).getMessage();
      if (msg != null) {
        msgVector.insertElementAt(msg, msgVector.size());
      }
    }
    return msgVector;
  }


  /** Like getSelectedMessageVector() but returns a Vector of ViewedMessages
      instead of a Vector of Messages. */
  synchronized Vector getSelectedViewedMessageVector() {
    Vector msgVector = new Vector();
    SelectionManager selection = fMessageTree.getSelectionManager();
    Enumeration messages = selection.getSelection();
    while (messages.hasMoreElements()) {
      TreePath path = (TreePath) messages.nextElement();
      ViewedMessage msg = (ViewedMessage) path.getTip();
      if (msg != null) {
        msgVector.insertElementAt(msg, msgVector.size());
      }
    }
    return msgVector;
  }


  /** Returns a Vector of all Messages (not including dummies.) */
  synchronized Vector getAllMessagesVector() {
    Vector v = new Vector();
    ViewedMessage m = fView.getMessageRoot();
    while (m != null) {
      getAllMessagesVector(m, v);
      m = m.getNext();
    }
    return v;
  }

  /** Writes the message and all of its children into the vector.
   */
  private synchronized void getAllMessagesVector(ViewedMessage m, Vector v) {
    if (!m.isDummy())
      v.insertElementAt(m.getMessage(), v.size());
    ViewedMessage kid = m.getChild();
    while (kid != null) {
      getAllMessagesVector(kid, v);
      kid = kid.getNext();
    }
  }


  //
  // OpenFolderAction class
  //

  class LoadFolderThread implements Runnable {
    Folder fFolder;

    public LoadFolderThread(Folder aFolder) {
      fFolder = aFolder;
    }

    public void run() {
      synchronized (fPanel) {
        if (fPanel.fFolder == fFolder) {
          fFolderLoadThread = null;
          return; // save ourselves some
        }

        fPanel.fFolder = fFolder;
      }
      if (fListeners != null) {
        fListeners.folderStatus(new StatusEvent(fPanel,
                              fLabels.getString("folderLoadingStatus")));
        fListeners.loadingFolder(new ChangeEvent(fPanel));
      }

      fMessageTree.setCursor(Cursor.getPredefinedCursor(Cursor.WAIT_CURSOR));
      fMessageTree.getSelectionManager().clearSelection();
      if (fFolder != null) {
        // Clear out the old folder manually
        synchronized (fPanel) {
          fMessageTree.setDataModel(null);
        }

        fView = FolderViewFactory.Make(fFolder);
        synchronized (fPanel) {
          fModel.setFolderView(fView);
          fMessageTree.setDataModel(fModel);
        }
        updateSortMenu();
      } else {
        synchronized (fPanel) {
          fMessageTree.setDataModel(null);
          fModel.setFolderView(null);
        }
      }
      fMessageTree.setCursor(Cursor.getDefaultCursor());
      synchronized (fPanel) {
        fFolderLoadThread = null;
      }

      if (fListeners != null) {
        fListeners.folderLoaded(new ChangeEvent(fPanel));
        fListeners.folderStatus(new StatusEvent(fPanel,
                             fLabels.getString("folderLoadedStatus")));
      }
    }
  }

  class OpenMessageAction extends UIAction implements Runnable {

    OpenMessageAction() {
      super("msgOpen");
    }

    public void actionPerformed(ActionEvent aEvent) {
      new Thread(this, "msgOpen").start();
    }

    public void run() {
      Vector selection = getSelectedMessageVector();
      MessageFrame frame = null;
      for (int i = 0; i < selection.size(); i++) {
        Message msg = (Message) selection.elementAt(i);
        frame = MessageFrame.FindMessageFrame(msg);
        if (frame != null) {
          frame.toFront();
        } else {
          frame = new MessageFrame(msg);
          frame.setVisible(true);
          frame.toFront();
        }
      }
      if (frame != null) {
        frame.requestFocus();
      }
    }
  }

  //
  // DeleteMessageAction class
  //

  class DeleteMessageAction extends UIAction {

    DeleteMessageAction() {
      super("msgDelete");
      this.setEnabled(false);
    }

    public void actionPerformed(ActionEvent aEvent) {
      ProgressFactory.DeleteMessagesProgress(getSelectedMessageVector());
    }
  }

  //
  // CopyMessageAction class
  //

  class CopyMessageAction extends UIAction {

    Folder fDest;

    CopyMessageAction(Folder aFolder) {
      super("msgCopy");

      fDest = aFolder;
    }

    public void actionPerformed(ActionEvent aEvent) {
      ProgressFactory.CopyMessagesProgress(getSelectedMessageVector(), fDest);
    }
  }

  //
  // MoveMessageAction class
  //

  class MoveMessageAction extends UIAction {

    Folder fDest;

    MoveMessageAction(Folder aFolder) {
      super("msgMove");

      fDest = aFolder;
    }

    public void actionPerformed(ActionEvent aEvent) {
      ProgressFactory.MoveMessagesProgress(getSelectedMessageVector(), fDest);
    }
  }

  //
  // ThreadAction class
  //

  class ThreadAction extends UIAction {

    boolean selected;

    public ThreadAction() {
      super("toggleThreading");
    }

    public void setSelected(boolean isSelected) {
      selected = isSelected;
    }

    public void actionPerformed(ActionEvent aEvent) {
      if (fView != null) {
        boolean selected = !fView.isThreaded();
        fView.setIsThreaded(selected);
        setSelected(selected ? true : false);
        fView.reThread();
      }
    }
  }

  //
  // SortAction class
  //

  class SortAction extends UIAction {
    int fType;
    boolean selected;

    public SortAction(String aAction, int aType) {
      super(aAction);
      fType = aType;
    }

    public int getType() {
      return fType;
    }
    
    public void setSelected(boolean isSelected) {
      selected = isSelected;
    }

    public void actionPerformed(ActionEvent aEvent) {
      String action = aEvent.getActionCommand();
      if (fView != null) {
        fView.prependSortOrder(fType);
        setSelected(true);
        fView.reThread();
      }
    }
  }

  //
  // ReplyAction class
  //

  class ReplyAction extends UIAction {
    boolean replyall;
    public ReplyAction(String aAction, boolean r) {
      super(aAction);
      this.setEnabled(false);

      replyall = r;
    }

    public void actionPerformed(ActionEvent aEvent) {
      Vector selection = getSelectedMessageVector();
      if (selection.size() != 1) {
        Assert.Assertion(false,
                         "Need to have exactly one message selected to reply");
      }
      Composition frame = new Composition();
      frame.initializeAsReply((Message) (selection.elementAt(0)), replyall);
      frame.show();
      frame.requestFocus();
    }
  }

  //
  // ForwardAction class
  //

  class ForwardAction extends UIAction {
    int fScope;

    ForwardAction(String aName, int aScope) {
      super(aName);
      fScope = aScope;

      this.setEnabled(aScope != kAttachment);
    }

    public void actionPerformed(ActionEvent aEvent) {
      Vector selection = getSelectedMessageVector();
      if (selection.size() != 1) {
        Assert.Assertion(false,
                         "Need to have exactly one message selected to reply");
      }
      Composition frame = new Composition();
      frame.initializeAsForward((Message) (selection.elementAt(0)), fScope);
      frame.show();
      frame.requestFocus();
    }
  }

  //
  // MarkAction class
  //

  class MarkAction extends UIAction {
    int fScope;

    MarkAction(String aName, int aScope) {
      super(aName);
      fScope = aScope;

      this.setEnabled(aScope == kAll);
    }

    public void actionPerformed(ActionEvent aEvent) {

      Message msgs[] = null;

      // gather the messages...
      //
      switch (fScope) {
      case kMessage:
        {
          Vector selection = getSelectedMessageVector();
          if (selection.size() == 0) break;
          msgs = new Message[selection.size()];
          Enumeration e = selection.elements();
          for (int i = 0; e.hasMoreElements(); i++)
            msgs[i] = (Message) e.nextElement();
        }
        break;

      case kThread:
        {
          Vector selection = getSelectedViewedMessageVector();
          if (selection.size() == 0) break;
          Vector thread_msgs = new Vector();
          for (Enumeration e = selection.elements(); e.hasMoreElements(); ) {
            ViewedMessage m = (ViewedMessage) e.nextElement();
            ViewedMessage p;
            // Move up to the root of the thread...
            while ((p = m.getParent()) != null)
              m = p;
            // Add the thread subtree to the vector.
            getAllMessagesVector(m, thread_msgs);
          }
          msgs = new Message[thread_msgs.size()];
          Enumeration e = thread_msgs.elements();
          for (int i = 0; e.hasMoreElements(); i++)
            msgs[i] = (Message) e.nextElement();
        }
        break;

      case kAll:
        {
          Vector all = getAllMessagesVector();
          if (all.size() == 0) break;
          msgs = new Message[all.size()];
          Enumeration e = all.elements();
          for (int i = 0; e.hasMoreElements(); i++)
            msgs[i] = (Message) e.nextElement();
        }
        break;
      }

      // ...then mark them read.
      //
      if (msgs != null) {
        try {
          fFolder.setFlags(msgs, new Flags(Flags.Flag.SEEN), true);
        } catch (MessagingException exc) {
          exc.printStackTrace();
        }
        }
      }
    }
  

  //
  // Cut-n-paste stuff
  //

  class FPClipboardOwner implements ClipboardOwner {
    public void lostOwnership(Clipboard aClipboard,
                              Transferable aTransferable) {
    }
  }

  class CopyToClipboardAction extends UIAction {

    CopyToClipboardAction() {
      super("copy-to-clipboard");
    }

    public void actionPerformed(ActionEvent aEvent) {
      System.out.println("FolderPanel: copy-to-clipboard");
      //      Clipboard cb = Toolkit.getDefaultToolkit().getSystemClipboard();
      Clipboard cb = fPrivateClipboard;

      Transferable transferable =
        new MessageListTransferable(getSelectedMessageVector());

      cb.setContents(transferable, new FPClipboardOwner());
    }
  }

  class PasteFromClipboardAction extends UIAction {

    PasteFromClipboardAction() {
      super("paste-from-clipboard");
    }

    public void actionPerformed(ActionEvent aEvent) {
      System.out.println("FolderPanel: paste-from-clipboard");
      //      Clipboard cb = Toolkit.getDefaultToolkit().getSystemClipboard();
      Clipboard cb = fPrivateClipboard;
      Transferable transferable = cb.getContents(fPanel);
      DataFlavor flavor = MessageListTransferable.GetDataFlavor();

      try {
        MessageListTransferable messageList =
          (MessageListTransferable) transferable.getTransferData(flavor);

        ProgressFactory.CopyMessagesProgress(messageList.getMessageList(),
                                             fFolder);
      } catch (UnsupportedFlavorException e) {
        System.err.println("FolderPanel: " + e);
      } catch (IOException e) {
        System.err.println("FolderPanel: " + e);
      }
    }
  }
}

//
// Multicaster class for FolderPanelListeners
//

class FolderMulticaster implements FolderPanelListener {
  FolderPanelListener a, b;

  public FolderMulticaster(FolderPanelListener a, FolderPanelListener b) {
    this.a = a;
    this.b = b;
  }

  public static FolderPanelListener add(FolderPanelListener a, FolderPanelListener b) {
    if (a == null) {
      return b;
    }
    if (b == null) {
      return a;
    }
    return new FolderMulticaster(a, b);
  }

  public static FolderPanelListener remove(FolderPanelListener a, FolderPanelListener b) {
    if (a == b || a == null) {
        return null;
    } else if (a instanceof FolderMulticaster) {
        return ((FolderMulticaster)a).remove(b);
    } else {
        return a;   // it's not here
    }
  }

  public FolderPanelListener remove(FolderPanelListener c) {
    if (c == a) {
      return b;
    }
    if (c == b) {
      return a;
    }
    FolderPanelListener a2 = remove(a, c);
    FolderPanelListener b2 = remove(b, c);
    if (a2 == a && b2 == b) {
        return this;  // it's not here
    }
    return add(a2, b2);
  }

  public void loadingFolder(ChangeEvent aEvent) {
    a.loadingFolder(aEvent);
    b.loadingFolder(aEvent);
  }

  public void folderLoaded(ChangeEvent aEvent) {
    a.folderLoaded(aEvent);
    b.folderLoaded(aEvent);
  }

  public void folderStatus(StatusEvent aEvent) {
    a.folderStatus(aEvent);
    b.folderStatus(aEvent);
  }

  public void folderSelectionChanged(ChangeEvent aEvent) {
     a.folderSelectionChanged(aEvent);
     b.folderSelectionChanged(aEvent);
  }

  public void folderSelectionDoubleClicked(ChangeEvent aEvent) {
     a.folderSelectionDoubleClicked(aEvent);
     b.folderSelectionDoubleClicked(aEvent);
  }
}
