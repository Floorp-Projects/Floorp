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

import java.awt.BorderLayout;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.datatransfer.Clipboard;
import java.awt.datatransfer.ClipboardOwner;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.awt.event.ActionEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseEvent;
import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.text.MessageFormat;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Locale;
import java.util.Properties;
import java.util.ResourceBundle;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.swing.BorderFactory;
import javax.swing.BoxLayout;
import javax.swing.Icon;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
//import javax.swing.JToolBar;
import javax.swing.JViewport;
import javax.swing.KeyStroke;
import javax.swing.ToolTipManager;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
//import javax.swing.plaf.BorderUIResource;

import calypso.util.ArrayEnumeration;
import calypso.util.Assert;
import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

//import netscape.orion.toolbars.NSToolbar;
//import netscape.orion.uimanager.AbstractUICmd;
//import netscape.orion.uimanager.IUICmd;

import javax.mail.Folder;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Store;

import grendel.composition.Composition;
import grendel.storage.FolderExtraFactory;
import grendel.storage.SearchResultsFolderFactory;
import grendel.ui.UIAction;
import grendel.view.ViewedFolder;
import grendel.view.ViewedStore;
import grendel.view.ViewedStoreEvent;
import grendel.view.ViewedStoreListener;
import grendel.widgets.CellEditor;
import grendel.widgets.Column;
import grendel.widgets.ColumnHeader;
import grendel.widgets.ColumnModel;
import grendel.widgets.ColumnChangeListener;
import grendel.widgets.DefaultCellRenderer;
import grendel.widgets.GrendelToolBar;
import grendel.widgets.SelectionEvent;
import grendel.widgets.SelectionListener;
import grendel.widgets.SelectionManager;
import grendel.widgets.StatusEvent;
import grendel.widgets.TextCellEditor;
import grendel.widgets.TreePath;
import grendel.widgets.TreeTable;
import grendel.widgets.TreeTableDataModel;
import grendel.widgets.TreeTableModelBroadcaster;
import grendel.widgets.TreeTableModelEvent;
import grendel.widgets.TreeTableModelListener;

/**
 * Panel to display the <em>contents</em> of a folder.
 */

public class MasterPanel extends GeneralPanel {
  TreeTable           fFolderTree;
  FolderModel         fModel = null;
  MasterPanelListener fListeners = null;
  MasterPanel         fPanel;
  SelectionListener   fSelectionListener = null;
  StoreChangeListener fStoreChangeListener = null;
  ViewedStore         fStores[];

  UIAction            fActions[] = {ActionFactory.GetNewMailAction(),
                                    ActionFactory.GetComposeMessageAction(),
                                    new CopyToClipboardAction(),
                                    new PasteFromClipboardAction(),
                                    new NewFolderAction(),
                                    new DeleteFolderAction()};

  public static final String  kNameID = new String("Name");
  public static final String  kUnreadID = new String("Unread");
  public static final String  kTotalID = new String("Total");

  /**
   * Constructs a new master panel.
   */

  public MasterPanel() {
    fPanel = this;
    JScrollPane scrollPane = new JScrollPane();
    //scrollPane.setBorder(BorderUIResource.getLoweredBevelBorderUIResource());
    scrollPane.setBorder(BorderFactory.createLoweredBevelBorder());
    Util.RegisterScrollingKeys(scrollPane);

    fFolderTree = new TreeTable(null);
    ToolTipManager.sharedInstance().registerComponent(fFolderTree);
    fFolderTree.setTreeColumn(kNameID);

    Column column;
    FolderCellRenderer renderer = new FolderCellRenderer();
    TextCellEditor editor = new TextCellEditor();

    column = new Column(kNameID, fLabels.getString("nameLabel"));
    column.setWidth(250);
    column.setCellRenderer(renderer);
    column.setCellEditor(editor);
    fFolderTree.addColumn(column);

    column = new Column(kUnreadID, fLabels.getString("unreadLabel"));
    column.setWidth(50);
    column.setCellRenderer(renderer);
    fFolderTree.addColumn(column);

    column = new Column(kTotalID, fLabels.getString("totalLabel"));
    column.setWidth(50);
    column.setCellRenderer(renderer);
    fFolderTree.addColumn(column);

    Preferences prefs = PreferencesFactory.Get();
    fFolderTree.getColumnModel().setPrefString(prefs.getString("mail.master_panel.column_layout", ""));

    registerKeyboardAction(new CopyToClipboardAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_C,
                                                  KeyEvent.CTRL_MASK),
                           WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);
    registerKeyboardAction(new PasteFromClipboardAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_V,
                                                  KeyEvent.CTRL_MASK),
                           WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    ColumnHeader columnHeader = fFolderTree.getColumnHeader();

    scrollPane.setColumnHeaderView(columnHeader);
    scrollPane.setViewportView(fFolderTree);

    add(scrollPane);

    // ###HACKHACKHACK  Remove me when javamail fixes their stuff.
    java.io.File mailcapfile = new java.io.File("mailcap");
    if (!mailcapfile.exists()) {
      try {
        (new java.io.RandomAccessFile(mailcapfile, "rw")).close();
        System.out.println("*** Created empty mailcap file in current");
        System.out.println("*** directory (to work around buggy javamail");
        System.out.println("*** software from JavaSoft).");
      } catch (java.io.IOException e) {
        System.out.println("*** Couldn't create empty mailcap file: " + e);
        System.out.println("*** Imminent crash is likely due to buggy");
        System.out.println("*** javamail software from JavaSoft.");
      }
    }

    fModel = new FolderModel();
    ViewedStore stores[] = StoreFactory.Instance().getStores();
    fStores = new ViewedStore[stores.length];
    for (int i = 0; i < stores.length; i++) {
      fModel.addStore(stores[i]);
      fStores[i] = stores[i];
    }

    fFolderTree.setDataModel(fModel);

    fSelectionListener = new MasterSelectionListener();
    fFolderTree.getSelectionManager().addSelectionListener(fSelectionListener);

    fStoreChangeListener = new StoreChangeListener();
    StoreFactory.Instance().addChangeListener(fStoreChangeListener);
    
    ActionFactory.SetComposeMessageThread(new ComposeMessageThread());
  }

  public void dispose() {
    Preferences prefs = PreferencesFactory.Get();
    prefs.putString("mail.master_panel.column_layout",
                    fFolderTree.getColumnModel().getPrefString());

    fFolderTree.getSelectionManager().removeSelectionListener(fSelectionListener);
    StoreFactory.Instance().removeChangeListener(fStoreChangeListener);
  }

  /**
   * Returns the actions associated with this panel.
   */

  public UIAction[] getActions() {
    return fActions;
  }

  /**
   * Returns the toolbar associated with this panel.
   */

  public GrendelToolBar getToolBar() {
    return buildToolBar("masterToolBar", fActions);
  }

  /**
   * Returns an enumeration of <code>TreePaths</code>s representing the
   * current selection. The tips of the paths are <code>Folder</code>s.
   */

  public Enumeration getSelection() {
    return fFolderTree.getSelectionManager().getSelection();
  }

  /**
   * Adds a MasterPanelListener
   */

  public void addMasterPanelListener(MasterPanelListener aListener) {
    fListeners = MasterMulticaster.add(fListeners, aListener);
  }

  /**
   * Removes a MasterPanelListener
   */

  public void removeMasterPanelListener(MasterPanelListener aListener) {
    fListeners = MasterMulticaster.remove(fListeners, aListener);
  }

  //
  // Component overloads
  //

  public boolean isOpaque() {
    return true;
  }

  synchronized ViewedFolder getSelectedViewedFolder() {
    ViewedFolder res = null;
    SelectionManager selection = fFolderTree.getSelectionManager();
    if (selection.getSelectionCount() == 1) {
      TreePath path = (TreePath) selection.getSelection().nextElement();
      res = GetViewedFolder(path.getTip());
    }
    return res;
  }

  synchronized Vector getSelectedFolderVector() {
    Vector folderVector = new Vector();
    SelectionManager selection = fFolderTree.getSelectionManager();
    Enumeration folders = selection.getSelection();
    while (folders.hasMoreElements()) {
      TreePath path = (TreePath) folders.nextElement();
      Object folder = path.getTip();
      if (folder != null) {
        folderVector.insertElementAt(folder, folderVector.size());
      }
    }
    return folderVector;
  }

  public static ViewedFolder GetViewedFolder(Object aObject) {
    ViewedFolder res = null;

    if (aObject instanceof ViewedFolder) {
      res = (ViewedFolder) aObject;
    }
    return res;
  }
  public static Folder getFolder(Object aObject) {
    Folder res = null;
    if (aObject instanceof Store) {
      try {
        res = ((Store) aObject).getDefaultFolder();
      } catch (MessagingException e) {}
    } else if (aObject instanceof Folder) {
      res = (Folder) aObject;
    } if (aObject instanceof ViewedFolder) {
      res = ((ViewedFolder) aObject).getFolder();
    }
    return res;
  }

  //
  // MasterSelectionListener class
  //

  class MasterSelectionListener implements SelectionListener {
    public void selectionChanged(SelectionEvent aEvent) {
      if (fListeners != null) {
        fListeners.masterSelectionChanged(new ChangeEvent(fPanel));
      }
    }

    public void selectionDoubleClicked(MouseEvent aEvent) {
      if (fListeners != null) {
        fListeners.masterSelectionDoubleClicked(new ChangeEvent(fPanel));
      }
    }
    public void selectionContextClicked(MouseEvent aEvent) {
    }
    public void selectionDragged(MouseEvent aEvent) {
    }
  }

  class NewFolderAction extends UIAction {

    NewFolderAction() {
      super("folderNew");
    }


    public void actionPerformed(ActionEvent aEvent) {
      new Thread(new NewFolderThread(), "NewFolder").start();
    }

    class NewFolderThread implements Runnable {
      public void run() {
        new NewFolderDialog(Util.GetParentFrame(fPanel),
                            getSelectedViewedFolder());
      }
    }
  }

  class DeleteFolderAction extends UIAction {
    DeleteFolderAction() {
      super("folderDelete");
    }

    public void actionPerformed(ActionEvent aEvent) {
      System.out.println("Delete Folder");
      ViewedFolder viewedFolder = getSelectedViewedFolder();
      if (viewedFolder != null) {
        Folder folder = viewedFolder.getFolder();
        System.out.println("Deleting Folder: " + folder.getName());
        try {
          folder.delete(true);
        } catch (MessagingException e) {
          e.printStackTrace();
        }
      }
    }
  }

  class ComposeMessageThread implements Runnable {
    public void run() {

      int identity;
      try {
        Preferences prefs = PreferencesFactory.Get();
        InetAddress ia = InetAddress.getByName(getSelectedViewedFolder().getViewedStore().getHost());
        String fPrefBase = "mail." + getSelectedViewedFolder().getViewedStore().getProtocol()
                    + "-" + ia.getHostName();
        System.out.println("fPrefBase");
        identity = prefs.getInt(fPrefBase + ".default-identity", 0);
      } catch (NullPointerException npe) {
      	identity = 0;
      } catch (UnknownHostException uhe) {
      	uhe.printStackTrace();
      	identity = 0;
      }

      ActionFactory.setIdent(identity);

      Composition frame = new Composition();
 
      frame.show();
      frame.requestFocus();
    }
  }

  class MPClipboardOwner implements ClipboardOwner {
    public void lostOwnership(Clipboard aClipboard,
                              Transferable aTransferable) {
    }
  }

  class CopyToClipboardAction extends UIAction {

    CopyToClipboardAction() {
      super("copy-to-clipboard");
    }

    public void actionPerformed(ActionEvent aEvent) {
      System.out.println("MasterPanel: copy-to-clipboard");
      //      Clipboard cb = Toolkit.getDefaultToolkit().getSystemClipboard();
      Clipboard cb = fPrivateClipboard;

      Transferable transferable =
        new FolderListTransferable(getSelectedFolderVector());

      cb.setContents(transferable, new MPClipboardOwner());
    }
  }

  class PasteFromClipboardAction extends UIAction {

    PasteFromClipboardAction() {
      super("paste-from-clipboard");
    }

    public void actionPerformed(ActionEvent aEvent) {
      System.out.println("MasterPanel: paste-from-clipboard");
      //      Clipboard cb = Toolkit.getDefaultToolkit().getSystemClipboard();
      Clipboard cb = fPrivateClipboard;
      Transferable transferable = cb.getContents(fPanel);

      DataFlavor msgFlavor = MessageListTransferable.GetDataFlavor();
      DataFlavor folderFlavor = FolderListTransferable.GetDataFlavor();
      ViewedFolder viewedFolder = getSelectedViewedFolder();
      Folder folder = null;
      if (viewedFolder == null) {
        return;
      } else {
        folder = viewedFolder.getFolder();
      }

      if (transferable.isDataFlavorSupported(msgFlavor)) {
        try {
          MessageListTransferable messageList =
            (MessageListTransferable) transferable.getTransferData(msgFlavor);

          ProgressFactory.CopyMessagesProgress(messageList.getMessageList(),
                                               folder);
        } catch (UnsupportedFlavorException e) {
          System.err.println("MasterPanel: " + e);
        } catch (IOException e) {
          System.err.println("MasterPanel: " + e);
        }
      } else if (transferable.isDataFlavorSupported(folderFlavor)) {
        try {
          FolderListTransferable messageList =
           (FolderListTransferable) transferable.getTransferData(folderFlavor);
        } catch (UnsupportedFlavorException e) {
          System.err.println("MasterPanel: " + e);
        } catch (IOException e) {
          System.err.println("MasterPanel: " + e);
        }
      }
    }
  }

  class StoreChangeListener implements ChangeListener {
    public void stateChanged(ChangeEvent aEvent) {
      int i;
      for (i = 0; i < fStores.length; i++) {
        fModel.removeStore(fStores[i]);
      }

      ViewedStore stores[] = StoreFactory.Instance().getStores();
      fStores = new ViewedStore[stores.length];

      for (i = 0; i < stores.length; i++) {
        fModel.addStore(stores[i]);
        fStores[i] = stores[i];
      }
    }
  }
}

//
// Multicaster for MasterPanelListeners
//

class MasterMulticaster implements MasterPanelListener {
  MasterPanelListener a, b;

  public MasterMulticaster(MasterPanelListener a, MasterPanelListener b) {
    this.a = a;
    this.b = b;
  }

  public static MasterPanelListener add(MasterPanelListener a, MasterPanelListener b) {
    if (a == null) {
      return b;
    }
    if (b == null) {
      return a;
    }
    return new MasterMulticaster(a, b);
  }

  public static MasterPanelListener remove(MasterPanelListener a, MasterPanelListener b) {
    if (a == b || a == null) {
        return null;
    } else if (a instanceof MasterMulticaster) {
        return ((MasterMulticaster)a).remove(b);
    } else {
        return a;   // it's not here
    }
  }

  public MasterPanelListener remove(MasterPanelListener c) {
    if (c == a) {
      return b;
    }
    if (c == b) {
      return a;
    }
    MasterPanelListener a2 = remove(a, c);
    MasterPanelListener b2 = remove(b, c);
    if (a2 == a && b2 == b) {
        return this;  // it's not here
    }
    return add(a2, b2);
  }

  public void masterSelectionChanged(ChangeEvent aEvent) {
     a.masterSelectionChanged(aEvent);
     b.masterSelectionChanged(aEvent);
  }
  public void masterSelectionDoubleClicked(ChangeEvent aEvent) {
     a.masterSelectionDoubleClicked(aEvent);
     b.masterSelectionDoubleClicked(aEvent);
  }
}

//
// FolderDataModel. Maps a Store into a TreeTableDataModel
//

class FolderModel implements TreeTableDataModel {
  Vector fStores = new Vector();
  TreeTableModelListener fListeners = null;
  Hashtable fCollapsed = new Hashtable();

  StoreObserver fStoreObserver;

  ResourceBundle fLabels = ResourceBundle.getBundle("grendel.ui.Labels");

  public FolderModel() {
    fStoreObserver = new StoreObserver();
  }

  public void addStore(ViewedStore aStore) {
    aStore.addViewedStoreListener(fStoreObserver);
    fStores.addElement(aStore);
    updateFolderCreated(aStore);
  }

  public void removeStore(ViewedStore aStore) {
    aStore.removeViewedStoreListener(fStoreObserver);

    fStores.removeElement(aStore);
    updateFolderDeleted(aStore);
  }

  public void addFolder(ViewedFolder aFolder) {
    fStores.addElement(aFolder);
    updateFolderCreated(aFolder);
  }

  public void removeFolder(ViewedFolder aFolder) {
    fStores.removeElement(aFolder);
    updateFolderDeleted(aFolder);
  }

  public boolean showRoot() {
    return true;
  }

  // Navigation stuff
  public Object getRoot() {
    if (fStores.size() > 0) {
      return fStores.elementAt(0);
    } else {
      return null;
    }
  }

  public boolean isLeaf(Object aNode) {
    ViewedFolder folder = (ViewedFolder) aNode;
    if (folder != null) {
      ViewedStore store = folder.getViewedStore();
      if (folder == store && !store.isConnected()) {
        return false;
      }
    }

    if (aNode == null) {
      return (getChildren(aNode) == null);
    } else {
      return (getChild(aNode) == null);
    }
  }

  public Enumeration getChildren(Object aNode) {
    if (aNode == null) {
      return fStores.elements();
    }
    return null;
  }

  public Object getChild(Object aNode) {
    if (aNode instanceof ViewedFolder) {
      return ((ViewedFolder) aNode).getFirstSubFolder();
    }
    return null;
  }

  public Object getNextSibling(Object aNode) {
    if (aNode instanceof ViewedFolder) {
      return ((ViewedFolder) aNode).getNextFolder();
    }
    return null;
  }

  // Attributes
  public void setCollapsed(TreePath aPath, boolean aCollapsed) {
    TreeTableModelEvent event =
      new TreeTableModelEvent(this, aPath);

    if (aCollapsed) {
      if (fCollapsed.remove(aPath) != null) {
        if (fListeners != null) {
          fListeners.nodeCollapsed(event);
        }
      }
    } else {
      if (fCollapsed.put(aPath, "x") == null) {
        if (fListeners != null) {
          fListeners.nodeExpanded(event);
        }
      }
    }
  }

  public boolean isCollapsed(TreePath aPath) {
    return !fCollapsed.containsKey(aPath);
  }

  public Object getData(Object aNode, Object aID) {
    ViewedFolder node = null;

    if (aNode instanceof ViewedStore) {
      if (aID == MasterPanel.kNameID) {
        String host = ((ViewedStore) aNode).getHost();
        if (host != null) {
          return MessageFormat.format(fLabels.getString("remoteStoreLabel"),
                                                        new Object[] {host});
        } else {
          return fLabels.getString("localStoreLabel");
        }
      }
      return "";
    }

    node = getViewedFolder(aNode);

    if (node == null) {
      return "";
    }

    if (aID == MasterPanel.kNameID) {
      return node.getFolder().getName();
    } else if (aID == MasterPanel.kUnreadID) {
      int n = node.getUnreadMessageCount();
      if (n < 0)
        return "???";
      else
        return Integer.toString(n);
    } else if (aID == MasterPanel.kTotalID) {
      int n = node.getUndeletedMessageCount();
      if (n < 0)
        return "???";
      else
        return Integer.toString(n);
    } else {
      throw new Error("unknown column");
    }
  }

  public Icon getIcon(Object aNode) {
    return UIFactory.Instance().getFolderIcon((ViewedFolder) aNode,
                                              false, false);
  }

  public Icon getOverlayIcon(Object aNode) {
    return UIFactory.Instance().getFolderOverlayIcon((ViewedFolder) aNode,
                                                     false, false);
  }

  public void setData(Object aNode, Object aID, Object aValue) {
    Folder parent = null, node = null;
    if (aValue.equals(getData(aNode, aID))) {
      return;
    }
    try {
    node = getFolder(aNode);

    if (aID == MasterPanel.kNameID) {
      
        parent = node.getParent();
    }
      String newName = (String) aValue;

      Folder newFolder = null;

      // Check name validity
      try {
        if (newName.indexOf(node.getSeparator()) >= 0) {
          Object args[] = {new String("" + node.getSeparator())};
          String err =
            MessageFormat.format(fLabels.getString("folderInvalidCharacter"),
                                 args);
          JOptionPane.showMessageDialog(null, err,
                                        fLabels.getString("folderCreateError"),
                                        JOptionPane.ERROR_MESSAGE);
          return;
        }

        if (parent != null) {
          newFolder = parent.getFolder(newName);
        } else {
          newFolder = node.getStore().getFolder(newName);
        }
      } catch (MessagingException e) {
        System.err.println("setData: " + e);
      }
      if (newFolder != null) {
        try {
        if (newFolder.exists()) {
          Object args[] = {newName};
          String err =
            MessageFormat.format(fLabels.getString("folderExistsError"),
                                 args);
          JOptionPane.showMessageDialog(null, err,
                                        fLabels.getString("folderCreateError"),
                                        JOptionPane.ERROR_MESSAGE);
        }
        } catch (MessagingException exc) {
        }
        } else {
          try {
            node.renameTo(newFolder);
          } catch (MessagingException e) {
            System.err.println("renameTo: " + e);
          }
        }
    } catch (MessagingException e) {
    }
    }
  

  Folder getFolder(Object aObject) {
    Folder res = null;
    if (aObject instanceof ViewedFolder) {
      res = ((ViewedFolder) aObject).getFolder();
    }
    return res;
  }

  ViewedFolder getViewedFolder(Object aObject) {
    ViewedFolder res = null;
    if (aObject instanceof ViewedFolder) {
      res = ((ViewedFolder) aObject);
    }
    return res;
  }

  public void addTreeTableModelListener(TreeTableModelListener aListener) {
    fListeners = TreeTableModelBroadcaster.add(fListeners, aListener);
  }

  public void removeTreeTableModelListener(TreeTableModelListener aListener) {
    fListeners = TreeTableModelBroadcaster.remove(fListeners, aListener);
  }

  TreePath createTreePath(ViewedFolder aNode) {
    Vector pathVector = new Vector();
    if (aNode != null) {
      while (aNode != null) {
        pathVector.insertElementAt(aNode, 0);
        aNode = aNode.getParentFolder();
      }
    }
    return new TreePath(pathVector);
  }

  TreePath createTreePath(Folder aFolder) {
    ViewedStore store =
      StoreFactory.Instance().getViewedStore(aFolder.getStore());
    if (store != null) {
      try {
        ViewedFolder folder = store.getViewedFolder(aFolder);
        if (folder != null) {
          return createTreePath(folder);
        }
      } catch (MessagingException e) {
        e.printStackTrace();
      }
    }
    return null;
  }

  void updateFolder(ViewedFolder aFolder) {
    TreePath path = createTreePath(aFolder);
    if (fListeners != null && path != null) {
      fListeners.nodeChanged(new TreeTableModelEvent(this, path, null));
    }
  }

  void updateFolderCreated(ViewedFolder aFolder) {
    TreePath path = createTreePath(aFolder.getParentFolder());
    if (fListeners != null && path != null) {
      fListeners.nodeInserted(new TreeTableModelEvent(this, path,
                                                      new Object[] {aFolder}));
    }
  }

  void updateFolderDeleted(ViewedFolder aFolder) {
    TreePath path = createTreePath(aFolder.getParentFolder());
    if (fListeners != null && path != null) {
      fListeners.nodeDeleted(new TreeTableModelEvent(this, path,
                                                     new Object[] {aFolder}));
    }
  }

  class StoreObserver implements ViewedStoreListener {
    public void folderCreated(ViewedStoreEvent aEvent) {
      updateFolderCreated(aEvent.getFolder());
    }

    public void folderDeleted(ViewedStoreEvent aEvent) {
      updateFolderDeleted(aEvent.getFolder());
    }

    public void folderChanged(ViewedStoreEvent aEvent) {
      updateFolder(aEvent.getFolder());
    }

    public void storeNotification(ViewedStoreEvent aEvent) {
    }
  }
}

//
// FolderCellRenderer Class
//

class FolderCellRenderer extends DefaultCellRenderer {
  Font          fPlain;
  Font          fBold;

  public FolderCellRenderer() {
    fPlain = Font.decode("SansSerif-12");
    fBold = Font.decode("SansSerif-bold-12");
  }

  public void paint(Graphics g) {
    Font font = fPlain;
    ViewedFolder f = null;
    if (fObject instanceof ViewedStore) {
      font = fBold;
    } else if (fObject instanceof ViewedFolder) {
      f = (ViewedFolder) fObject;
    }
    if (f != null) {
      font = f.getUnreadMessageCount() == 0 ? fPlain : fBold;
    }
    setFont(font);

    super.paint(g);
  }
}
