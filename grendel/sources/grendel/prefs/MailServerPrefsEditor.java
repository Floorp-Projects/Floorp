/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Created: Will Scullin <scullin@netscape.com>, 16 Oct 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
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
import java.io.File;

import javax.mail.Store;
import javax.mail.URLName;

import javax.swing.AbstractListModel;
import javax.swing.JFileChooser;
import javax.swing.filechooser.FileFilter;
import javax.swing.JComponent;
import javax.swing.JList;
import javax.swing.JScrollPane;
import javax.swing.ListModel;
import javax.swing.text.JTextComponent;
import javax.swing.border.BevelBorder;
import javax.swing.event.EventListenerList;
import javax.swing.event.ListDataEvent;
import javax.swing.event.ListDataListener;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.table.AbstractTableModel;

import grendel.ui.EditHostDialog;
import grendel.ui.Util;
import grendel.ui.PageModel;
import grendel.ui.PageUI;

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
      setStore(fValues);
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
        JList c;
        c = (JList)fPanel.getCtrlByName(kHostListKey);

        URLName value = (URLName)c.getSelectedValue();
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
        JList c;
        c = (JList)fPanel.getCtrlByName(kHostListKey);

        URLName value = (URLName) c.getSelectedValue();
        if (value != null) {
          fHostListModel.remove(value);
          fPrefs.setStores(fHostListModel.getStores());
          fListeners.firePropertyChange(null, null, fPrefs);
        }
      } else if (action.equals(kChooseKey)) {
        JFileChooser chooser = new JFileChooser(fPrefs.getMailDirectory());
        DirectoryFilter filter = new DirectoryFilter();
        File selected;
        String path;
        chooser.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
        chooser.setFileFilter(filter);
        chooser.addChoosableFileFilter(filter);
        chooser.showDialog(fPanel, "Okay");
        selected = chooser.getSelectedFile();
        path = selected.getAbsolutePath();
        ((JTextComponent)
         fPanel.getCtrlByName(kMailDirectoryKey)).setText(path);
        fPrefs.setMailDirectory(path);
        fListeners.firePropertyChange(null, null, fPrefs);
      }
    }
  }

  class DirectoryFilter extends FileFilter {
    public boolean accept(File f) {
      return f.isDirectory();
    }

    public String getDescription() {
      return "Directories";
    }
  }

  public MailServerPrefsEditor() {
    fModel = new ServerPrefsModel();
    fHostListModel = new HostListModel();

    URL url = getClass().getResource("PrefDialogs.xml");
    //XXXrlk: why does this constructor have parameters?
//    fPanel = new PageUI(url, "id", "serverPrefs", fModel, getClass());
    fPanel = new PageUI();

    JComponent c;
    ChangeAction ca = new ChangeAction();
    Prefs prefs = new Prefs();
    MailServerPrefs mail = prefs.getMailServerPrefs();
    setValue(mail);

    c = fPanel.getCtrlByName(kSMTPHostKey);
    c.addPropertyChangeListener(ca);

    c = fPanel.getCtrlByName(kMailDirectoryKey);
    c.addPropertyChangeListener(ca);

    c = fPanel.getCtrlByName(kChooseKey);
    c.addPropertyChangeListener(ca);

    c = fPanel.getCtrlByName(kHostListKey);
    ((JList)c).setModel(fHostListModel);
    ((JList)c).addListSelectionListener(new ListListener());

    c = fPanel.getCtrlByName(kEditKey);
    c.setEnabled(false);
    c = fPanel.getCtrlByName(kDeleteKey);
    c.setEnabled(false);
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

  class ListListener
    implements ListSelectionListener {
    public void valueChanged(ListSelectionEvent e) {
      System.out.println("foo");
      JComponent c;
      c = (JComponent)e.getSource();
      boolean enabled = !(((JList)c).isSelectionEmpty());
      System.out.println(((JList)c).getSelectedValue());

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
    }

    public void remove(URLName aURLName) {
      int idx = fEditableStores.indexOf(aURLName);
      if (idx != -1) {
        fEditableStores.removeElementAt(idx);
        fireIntervalRemoved(this, idx, idx);
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
      fListeners.firePropertyChange(null, null, fPrefs);
    }

    public void propertyChange(PropertyChangeEvent aEvent) {
      event(aEvent);
    }
  }
}
