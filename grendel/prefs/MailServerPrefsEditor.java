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
 * Created: Will Scullin <scullin@netscape.com>, 16 Oct 1997.
 */

package grendel.prefs;

import java.awt.Component;
import java.awt.Graphics;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.beans.PropertyChangeSupport;
import java.beans.PropertyEditor;
import java.net.URL;
import java.util.EventObject;
import java.util.Hashtable;
import java.util.MissingResourceException;
import java.util.ResourceBundle;
import java.util.Vector;

import javax.mail.Store;
import javax.mail.URLName;

import javax.swing.AbstractListModel;
import javax.swing.FileType;
import javax.swing.JFileChooser;
//import java.awt.FileDialog;
//import java.io.File;
//import java.io.FilenameFilter;
import javax.swing.JList;
import javax.swing.JScrollPane;
import javax.swing.ListModel;
import javax.swing.border.BevelBorder;
import javax.swing.event.EventListenerList;
import javax.swing.event.ListDataEvent;
import javax.swing.event.ListDataListener;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.table.AbstractTableModel;

//import netscape.orion.dialogs.AbstractCtrl;
//import netscape.orion.dialogs.AttrNotFoundException;
//import netscape.orion.dialogs.JLISTeditor;
//import netscape.orion.dialogs.PageModel;
//import netscape.orion.dialogs.PageUI;

//import xml.tree.XMLNode;
//import xml.tree.TreeBuilder;

import grendel.ui.EditHostDialog;
import grendel.ui.Util;

public class MailServerPrefsEditor implements PropertyEditor
{
  MailServerPrefs fPrefs = new MailServerPrefs();
  PropertyChangeSupport fListeners = new PropertyChangeSupport(this);

  ResourceBundle fLabels =
    ResourceBundle.getBundle("grendel.prefs.PrefLabels");

  final static String kMailDirectoryKey = "mailDirectoryField";
  final static String kLeaveOnServerKey = "leaveOnServerCheck";
  final static String kSMTPHostKey = "smtpHostField";
  final static String kHostListKey = "hostList";
  final static String kNewKey = "newButton";
  final static String kEditKey = "editButton";
  final static String kDeleteKey = "deleteButton";
  final static String kChooseKey = "chooseButton";

  Hashtable fValues = null;
  HostListModel fHostListModel = null;

  PageUI fPanel;

  ServerPrefsModel fModel;

  class ServerPrefsModel extends PageModel {
    public ServerPrefsModel() {
      fValues = new Hashtable();
      fValues.put(kMailDirectoryKey, "");
      fValues.put(kLeaveOnServerKey, Boolean.TRUE);
      fValues.put(kSMTPHostKey, "");
    }

    public Object getAttribute(String aAttrib) throws AttrNotFoundException {
      Object res = fValues.get(aAttrib);
      if (res == null) {
        res = fLabels.getString(aAttrib);
      }
      if (res == null) {
        throw new AttrNotFoundException(aAttrib);
      }
      return res;
    }

    public void setAttribute(String aAttrib, Object aValue) {
      if (fValues.containsKey(aAttrib)) {
        fValues.put(aAttrib, aValue);
      }
    }

    public void actionPerformed(ActionEvent aEvent) {
      String action = aEvent.getActionCommand();
      if (action.equals(kNewKey)) {
        EditHostDialog hostDialog =
          new EditHostDialog(Util.GetParentFrame(fPanel), null);

        if (hostDialog.getURLName() != null) {
          fHostListModel.add(hostDialog.getURLName());
          fPrefs.setStores(fHostListModel.getStores());
          fListeners.firePropertyChange(null, null, fPrefs);
        }
      } else if (action.equals(kEditKey)) {
        AbstractCtrl c;
        c = fPanel.getCtrlByName(kHostListKey);

        URLName value = (URLName) c.getValue();
        if (value != null) {
          EditHostDialog hostDialog =
            new EditHostDialog(Util.GetParentFrame(fPanel), value);

          if (hostDialog.getURLName() != null) {
            fHostListModel.update(value, hostDialog.getURLName());
            fPrefs.setStores(fHostListModel.getStores());
            fListeners.firePropertyChange(null, null, fPrefs);
          }
        }
      } else if (action.equals(kDeleteKey)) {
        AbstractCtrl c;
        c = fPanel.getCtrlByName(kHostListKey);

        URLName value = (URLName) c.getValue();
        if (value != null) {
          fHostListModel.remove(value);
          fPrefs.setStores(fHostListModel.getStores());
          fListeners.firePropertyChange(null, null, fPrefs);
        }
      } else if (action.equals(kChooseKey)) {
        JFileChooser chooser = new JFileChooser(fPrefs.getMailDirectory());
        chooser.setChoosableFileTypes(new FileType[] {FileType.SharedFolder});
        chooser.showDialog(fPanel);
      }
    }
  }

  public MailServerPrefsEditor() {
    fModel = new ServerPrefsModel();
    fHostListModel = new HostListModel();

    XMLNode root = null;
    URL url = getClass().getResource("PrefDialogs.xml");
    try {
      root = xml.tree.TreeBuilder.build(url, getClass());
      XMLNode editHost = root.getChild("dialog", "id", "serverPrefs");

      fPanel = new PageUI(url, editHost, fModel);

      AbstractCtrl c;
      ChangeAction ca = new ChangeAction();

      c = fPanel.getCtrlByName(kSMTPHostKey);
      c.addPropertyChangeListener(ca);

      c = fPanel.getCtrlByName(kMailDirectoryKey);
      c.addPropertyChangeListener(ca);

      c = fPanel.getCtrlByName(kHostListKey);
      if (c instanceof JLISTeditor) {
        ((JLISTeditor) c).setModel(fHostListModel);
      }
      c.addPropertyChangeListener(new ListListener());

    } catch (Exception e) {
      e.printStackTrace();
    }
  }

  public String getAsText() {
    return null;
  }

  public Component getCustomEditor() {
    return fPanel;
  }

  public String getJavaInitializationString() {
    return "";
  }

  public String[] getTags() {
    return null;
  }

  public Object getValue() {
    fPanel.saveAll();

    fPrefs.setMailDirectory((String) fValues.get(kMailDirectoryKey));
    fPrefs.setLeaveOnServer(((Boolean) fValues.get(kLeaveOnServerKey)).booleanValue());
    fPrefs.setSMTPHost((String) fValues.get(kSMTPHostKey));

    fPrefs.setStores(fHostListModel.getStores());
    return fPrefs;
  }

  public boolean isPaintable() {
    return false;
  }

  public void paintValue (Graphics g, Rectangle r) {
  }

  public void setAsText(String aValue) {
  }

  public void setValue(Object aValue) {
    if (aValue instanceof MailServerPrefs) {
      MailServerPrefs oldPrefs = fPrefs;
      fPrefs = (MailServerPrefs) aValue;

      fValues.put(kMailDirectoryKey, fPrefs.getMailDirectory());
      fValues.put(kLeaveOnServerKey, fPrefs.getLeaveOnServer() ?
                  Boolean.TRUE : Boolean.FALSE);
      fValues.put(kSMTPHostKey, fPrefs.getSMTPHost());

      fHostListModel.setStores(fPrefs.getStores());

      fPanel.initAll();

      fListeners.firePropertyChange(null, oldPrefs, fPrefs);
    }
  }

  public boolean supportsCustomEditor() {
    return true;
  }

  public void addPropertyChangeListener(PropertyChangeListener l) {
    fListeners.addPropertyChangeListener(l);
  }

  public void removePropertyChangeListener(PropertyChangeListener l) {
    fListeners.removePropertyChangeListener(l);
  }

  class ListListener implements PropertyChangeListener {
    public void propertyChange(PropertyChangeEvent e) {
      AbstractCtrl c;
      c = (AbstractCtrl) e.getSource();
      boolean enabled = c.getValue() != null;

      c = fPanel.getCtrlByName(kDeleteKey);
      c.setEnabled(enabled);

      c = fPanel.getCtrlByName(kEditKey);
      c.setEnabled(enabled);
    }
  }


  class HostListModel extends AbstractListModel {
    URLName fStores[];
    Vector fEditableStores = new Vector();
    EventListenerList fListeners = new EventListenerList();

    HostListModel() {
      fStores = new URLName[0];
    }

    HostListModel(URLName aStores[]) {
      fStores = aStores;
    }

    public void setStores(URLName aStores[]) {
      int i;
      fStores = aStores;
      for (i = 0; i < aStores.length; i++) {
        if (aStores[i] != null) {
          fEditableStores.addElement(aStores[i]);
        }
      }

      fireContentsChanged(this, 0, fEditableStores.size() - 1);
    }

    public URLName[] getStores() {
      URLName res[] = new URLName[fEditableStores.size()];
      fEditableStores.copyInto(res);
      return res;
    }

    public void update(URLName aOld, URLName aNew) {
      if (!aOld.equals(aNew)) {
        int idx = fEditableStores.indexOf(aOld);
        fEditableStores.removeElementAt(idx);
        fEditableStores.insertElementAt(aNew, idx);

        fireContentsChanged(this, idx, idx);
      }
    }

    public void add(URLName aURLName) {
      fEditableStores.addElement(aURLName);

      fireIntervalAdded(this, fEditableStores.size() - 1,
                        fEditableStores.size() - 1);

      //fScrollPane.validate();
    }

    public void remove(URLName aURLName) {
      int idx = fEditableStores.indexOf(aURLName);
      if (idx != -1) {
        fEditableStores.removeElementAt(idx);

        fireIntervalRemoved(this, idx, idx);
        //fScrollPane.validate();
      }
    }

    public int getSize() {
      return fEditableStores.size();
    }

    public Object getElementAt(int index) {
      return fEditableStores.elementAt(index);
    }
  }

  class ChangeAction implements PropertyChangeListener {
    void event(EventObject aEvent) {
      getValue();
      fListeners.firePropertyChange(null, null, fPrefs);
    }

    public void propertyChange(PropertyChangeEvent aEvent) {
      event(aEvent);
    }
  }
}
