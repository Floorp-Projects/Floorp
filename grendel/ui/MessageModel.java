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
 * Created: Will Scullin <scullin@netscape.com>, 17 Dec 1997.
 */

package grendel.ui;

import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

import javax.swing.Icon;
import javax.swing.ImageIcon;

import javax.mail.Message;
import javax.mail.MessagingException;

import calypso.util.Assert;

import grendel.storage.MessageExtra;
import grendel.storage.MessageExtraFactory;
import grendel.view.FolderView;
import grendel.view.MessageSetViewObserver;
import grendel.view.ViewedMessage;
import grendel.widgets.TreePath;
import grendel.widgets.TreeTableDataModel;
import grendel.widgets.TreeTableModelBroadcaster;
import grendel.widgets.TreeTableModelEvent;
import grendel.widgets.TreeTableModelListener;


//
// MessageModel. Maps a FolderView into a TreeTableDataModel.
//

class MessageModel implements TreeTableDataModel {
  Hashtable              fCollapsed = new Hashtable();
  FolderView             fView = null;
  TreeTableModelListener fListeners;
  ViewObserver           fViewObserver = new ViewObserver();

  Icon                   fMessageIcon;
  Icon                   fMessageReadIcon;

  public MessageModel() {
    fMessageIcon = new ImageIcon(getClass().getResource("images/msg-small.gif"));
    fMessageReadIcon = new ImageIcon(getClass().getResource("images/msgRead-small.gif"));
  }

  public void setFolderView(FolderView aView) {
    if (fView != null) {
      fView.removeObserver(fViewObserver);
    }
    fView = aView;
    if (fView != null) {
      fView.addObserver(fViewObserver);
    }
  }

  public boolean showRoot() {
    return true;
  }

  // Navigation stuff
  public Object getRoot() {
    return fView.getMessageRoot();
  }

  public boolean isLeaf(Object aNode) {
    ViewedMessage node = (ViewedMessage) aNode;

    return node.getChild() == null;
  }

  public Enumeration getChildren(Object aNode) {
    return null;
  }

  public Object getChild(Object aNode) {
    ViewedMessage node = (ViewedMessage) aNode;

    return node.getChild();
  }

  public Object getNextSibling(Object aNode) {
    ViewedMessage node = (ViewedMessage) aNode;

    return node.getNext();
  }

  // Attributes
  public void setCollapsed(TreePath aPath, boolean aCollapsed) {
    TreeTableModelEvent event =
      new TreeTableModelEvent(this, aPath);

    if (aCollapsed) {
      if (fCollapsed.put(aPath, "x") == null) {
        if (fListeners != null) {
          fListeners.nodeCollapsed(event);
        }
      }
    } else {
      if (fCollapsed.remove(aPath) != null) {
        if (fListeners != null) {
          fListeners.nodeExpanded(event);
        }
      }
    }
  }

  public boolean isCollapsed(TreePath aPath) {
    return fCollapsed.containsKey(aPath);
  }

  public Object getData(Object aNode, Object aID) {
    ViewedMessage node = (ViewedMessage) aNode;

    if (node.isDummy()) {
      Message m = node.getChild().getMessage();
      if (aID == FolderPanel.kReadID)
        return Boolean.TRUE;
      else if (aID == FolderPanel.kSubjectID) {
        return Util.GetSubject(m);
      }
      return "";
    }

    Message m = node.getMessage();
    Assert.Assertion(m != null);

    if (m == null) {
      return "";
    } else if (aID == FolderPanel.kSubjectID) {
      return Util.GetSubject(m);
    } else {
      MessageExtra mextra = MessageExtraFactory.Get(m);
      Object result = null;
      try {
        if (aID == FolderPanel.kSenderID) {
          result = mextra.getAuthor();
        } else if (aID == FolderPanel.kDateID) {
          result = mextra.simplifiedDate();
        } else if (aID == FolderPanel.kReadID) {
          result = mextra.isRead() ? Boolean.TRUE : Boolean.FALSE;
        } else if (aID == FolderPanel.kFlagID) {
          result = mextra.isFlagged() ? Boolean.TRUE : Boolean.FALSE;
        } else if (aID == FolderPanel.kDeletedID) {
          result = mextra.isDeleted() ? Boolean.TRUE : Boolean.FALSE;
        } else {
          throw new Error("unknown column");
        }
      } catch (MessagingException e) {
        result = "???";           // ### Is this reasonable?
      }
      if (result == null) result = "";
      return result;
    }
  }

  public void setData(Object aNode, Object aID, Object aValue) {
    ViewedMessage node = (ViewedMessage) aNode;
    Message m = node.getMessage();
    if (m != null) {
      MessageExtra mextra = MessageExtraFactory.Get(m);
      boolean b = ((Boolean) aValue).booleanValue();

      try {
        if (aID == FolderPanel.kReadID) {
          System.out.println("marking " + Util.GetSubject(m) + " read " + b);
          mextra.setIsRead(b);
        } else if (aID == FolderPanel.kFlagID) {
          mextra.setFlagged(b);
        } else if (aID == FolderPanel.kDeletedID) {
          mextra.setDeleted(b);
        }
      } catch (MessagingException e) {
        // ### Shouldn't we report the error to the user or something?
      }
    }
  }

  public Icon getIcon(Object aNode) {
    ViewedMessage node = (ViewedMessage) aNode;
    Message m = node.getMessage();
    if (m == null) {
      return fMessageIcon;
    } else {
      try {
        if (MessageExtraFactory.Get(m).isRead()) {
          return fMessageReadIcon;
        }
      } catch (MessagingException e) {
      }
      return fMessageIcon;
    }
  }

  public Icon getOverlayIcon(Object aNode) {
    return null;
  }

  public void addTreeTableModelListener(TreeTableModelListener aListener) {
    fListeners = TreeTableModelBroadcaster.add(fListeners, aListener);
  }

  public void removeTreeTableModelListener(TreeTableModelListener aListener) {
    fListeners = TreeTableModelBroadcaster.remove(fListeners, aListener);
  }

  TreePath createTreePath(ViewedMessage aNode) {
    Vector pathVector = new Vector();

    while (aNode != null) {
      pathVector.insertElementAt(aNode, 0);
      aNode = aNode.getParent();
    }

    return new TreePath(pathVector);
  }

  class ViewObserver implements MessageSetViewObserver {
    public void messagesChanged(Enumeration inserted,
                                Enumeration deleted,
                                Enumeration changed) {
      if (fListeners != null) {
        Object children[] = new Object[1];
        ViewedMessage node;
        TreePath path;

        if (inserted != null) {
          System.out.println("Inserted messages");
          while (inserted.hasMoreElements()) {
            node = (ViewedMessage) inserted.nextElement();

            path = createTreePath(node.getParent());
            children[0] = node;

            fListeners.nodeInserted(new TreeTableModelEvent(this, path, children));
          }
        }
        if (deleted != null) {
          System.out.println("Deleted messages");
          while (deleted.hasMoreElements()) {
            node = (ViewedMessage) deleted.nextElement();

            path = createTreePath(node.getParent());
            children[0] = node;

            fListeners.nodeDeleted(new TreeTableModelEvent(this, path, children));
          }
        }
        if (changed != null) {
          while (changed.hasMoreElements()) {
            node = (ViewedMessage) changed.nextElement();

            Message m = node.getMessage();
            System.out.println("'" + Util.GetSubject(m) + "' changed");

            path = createTreePath(node);
            fListeners.nodeChanged(new TreeTableModelEvent(this, path, null));
          }
        }
      }
    }
  }
}
