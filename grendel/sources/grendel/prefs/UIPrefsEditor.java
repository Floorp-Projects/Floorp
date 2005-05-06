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
 * Created: Will Scullin <scullin@netscape.com>, 23 Dec 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 */

package grendel.prefs;

import java.awt.Component;
import java.awt.Graphics;
import java.awt.Insets;
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

import javax.swing.JList;
import javax.swing.JComponent;
import javax.swing.AbstractListModel;
import javax.swing.LookAndFeel;
import javax.swing.UIManager;
import javax.swing.JPanel;

import grendel.ui.XMLPageBuilder;
import grendel.ui.PageModel;
import grendel.ui.PageUI;

public class UIPrefsEditor extends JPanel
  implements PropertyEditor {
  UIPrefs fPrefs = new UIPrefs();
  PropertyChangeSupport fListeners = new PropertyChangeSupport(this);

  ResourceBundle fLabels =
    ResourceBundle.getBundle("grendel.prefs.PrefLabels");

  static final String kLAFKey="LAFList";

  Hashtable fValues = null;

  PageUI fPanel;

  UIPrefsModel fModel;

  class UIPrefsModel extends PageModel {
    public UIPrefsModel() {
      fValues = new Hashtable();
      setStore(fValues);
    }

    public void actionPerformed(ActionEvent aEvent) {
    }
  }

  public UIPrefsEditor() {
    fModel = new UIPrefsModel();

    // XMLNode root = null;
    URL url = getClass().getResource("PrefDialogs.xml");
    //XXXrlk: why does this constructor have parameters?
//    fPanel = new PageUI(url, "id", "UIPrefs", fModel, getClass());
    fPanel = new PageUI();

    JComponent c;
    ChangeAction ca = new ChangeAction();

    c = fPanel.getCtrlByName(kLAFKey);
    c.addPropertyChangeListener(ca);
    if (c instanceof JList) {
      ((JList) c).setModel(new LAFListModel());
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

    JList l = (JList)fPanel.getCtrlByName(kLAFKey);
    String str = (String)l.getSelectedValue();

    // sigh. we now search for the LAF
    UIManager.LookAndFeelInfo[] info =
      UIManager.getInstalledLookAndFeels();
    LookAndFeel lf = null;

    for (int i = 0; i < info.length; i++) {
      try {
        String cn = info[i].getClassName();
        Class c = Class.forName(cn);
        LookAndFeel current = (LookAndFeel)c.newInstance();
        if (current.getDescription().equals(str)) {
          lf = current;
        }
      } catch (Exception e) {
      }
    }
    if (lf != null) {    // fPrefs.setLAF((LookAndFeel) c.getValue());
      fPrefs.setLAF(lf);
    }

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
    if (aValue instanceof UIPrefs) {
      UIPrefs oldPrefs = fPrefs;
      fPrefs = (UIPrefs) aValue;

      JList l = (JList)fPanel.getCtrlByName(kLAFKey);
      l.setSelectedValue(fPrefs.getLAF().getDescription(), true);

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

  class ChangeAction implements PropertyChangeListener {
    void event(EventObject aEvent) {
      getValue();
      fListeners.firePropertyChange(null, null, fPrefs);
    }

    public void propertyChange(PropertyChangeEvent aEvent) {
      event(aEvent);
    }
  }

  class LAFListModel extends AbstractListModel {
    LookAndFeel fLAFs[] = null;

    LAFListModel() {
      UIManager.LookAndFeelInfo[] info =
        UIManager.getInstalledLookAndFeels();
      fLAFs = new LookAndFeel[info.length];
      for (int i = 0; i < info.length; i++) {
        try {
          String name = info[i].getClassName();
          Class c = Class.forName(name);
          fLAFs[i] = (LookAndFeel)c.newInstance();
        } catch (Exception e){
        }
      }
    }

    public int getSize() {
      if (fLAFs != null) {
        return fLAFs.length;
      }
      return 0;
    }

    public Object getElementAt(int index) {
      if (fLAFs != null && index < fLAFs.length) {
        // this is a hack. the toString() returns a string which is
        // best described as "unwieldly"
        return fLAFs[index].getDescription();
      }
      return null;
    }
  }

  public static void main(String[] args) throws Exception {
    javax.swing.JFrame frame = new javax.swing.JFrame("Foo bar");
    UIPrefsEditor ui = new UIPrefsEditor();
    frame.getContentPane().add(ui.fPanel);
    frame.pack();
    frame.setVisible(true);
  }

}
