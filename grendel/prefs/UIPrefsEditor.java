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
 * Created: Will Scullin <scullin@netscape.com>, 23 Dec 1997.
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

import javax.swing.AbstractListModel;
import javax.swing.LookAndFeel;
import javax.swing.UIManager;

//import netscape.orion.dialogs.AbstractCtrl;
//import netscape.orion.dialogs.AttrNotFoundException;
//import netscape.orion.dialogs.JLISTeditor;
//import netscape.orion.dialogs.PageModel;
//import netscape.orion.dialogs.PageUI;

//import xml.tree.XMLNode;
//import xml.tree.TreeBuilder;

public class UIPrefsEditor implements PropertyEditor {
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
    }
  }

  public UIPrefsEditor() {
    fModel = new UIPrefsModel();

    XMLNode root = null;
    URL url = getClass().getResource("PrefDialogs.xml");
    try {
      root = xml.tree.TreeBuilder.build(url, getClass());
      XMLNode editHost = root.getChild("dialog", "id", "UIPrefs");

      fPanel = new PageUI(url, editHost, fModel);

      AbstractCtrl c;
      ChangeAction ca = new ChangeAction();

      c = fPanel.getCtrlByName(kLAFKey);
      c.addPropertyChangeListener(ca);
      if (c instanceof JLISTeditor) {
        ((JLISTeditor) c).setModel(new LAFListModel());
      }

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

    AbstractCtrl c = fPanel.getCtrlByName(kLAFKey);
    fPrefs.setLAF((LookAndFeel) c.getValue());

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

      AbstractCtrl c = fPanel.getCtrlByName(kLAFKey);
      c.setValue(fPrefs.getLAF());

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
      System.out.println("propertyChange" + aEvent);
      event(aEvent);
    }
  }

  class LAFListModel extends AbstractListModel {
    LookAndFeel fLAFs[] = null;

    LAFListModel() {
      /* Hack until this works
      fLAFs = UIManager.getAuxiliaryLookAndFeels();
      */
      fLAFs = new LookAndFeel[] {
        new javax.swing.jlf.JLFLookAndFeel(),
        new javax.swing.motif.MotifLookAndFeel(),
        new javax.swing.windows.WindowsLookAndFeel()
      };
    }

    public int getSize() {
      if (fLAFs != null) {
        return fLAFs.length;
      }
      return 0;
    }

    public Object getElementAt(int index) {
      if (fLAFs != null && index < fLAFs.length) {
        return fLAFs[index];
      }
      return null;
    }
  }

}
