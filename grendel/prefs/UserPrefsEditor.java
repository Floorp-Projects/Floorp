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

//import netscape.orion.dialogs.AbstractCtrl;
//import netscape.orion.dialogs.AttrNotFoundException;
//import netscape.orion.dialogs.PageModel;
//import netscape.orion.dialogs.PageUI;

//import xml.tree.XMLNode;
//import xml.tree.TreeBuilder;

public class UserPrefsEditor implements PropertyEditor {
  UserPrefs fPrefs = new UserPrefs();
  PropertyChangeSupport fListeners = new PropertyChangeSupport(this);

  ResourceBundle fLabels =
    ResourceBundle.getBundle("grendel.prefs.PrefLabels");

  static final String kNameKey="userNameField";
  static final String kOrganizationKey="userOrganizationField";
  static final String kEmailAddressKey="userEmailAddressField";

  Hashtable fValues = null;

  PageUI fPanel;

  UserPrefsModel fModel;

  class UserPrefsModel extends PageModel {
    public UserPrefsModel() {
      fValues = new Hashtable();
      fValues.put(kNameKey, "");
      fValues.put(kOrganizationKey, "");
      fValues.put(kEmailAddressKey, "");
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

  public UserPrefsEditor() {
    fModel = new UserPrefsModel();
    XMLNode root = null;
    URL url = getClass().getResource("PrefDialogs.xml");
    try {
      root = xml.tree.TreeBuilder.build(url, getClass());
      XMLNode editHost = root.getChild("dialog", "id", "userPrefs");

      fPanel = new PageUI(url, editHost, fModel);

      AbstractCtrl c;
      ChangeAction ca = new ChangeAction();

      c = fPanel.getCtrlByName(kNameKey);
      c.addPropertyChangeListener(ca);

      c = fPanel.getCtrlByName(kOrganizationKey);
      c.addPropertyChangeListener(ca);

      c = fPanel.getCtrlByName(kEmailAddressKey);
      c.addPropertyChangeListener(ca);
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

    String name = (String) fValues.get(kNameKey);
    String org = (String) fValues.get(kOrganizationKey);
    String email = (String) fValues.get(kEmailAddressKey);

    fPrefs.setUserName(name);
    fPrefs.setUserOrganization(org);
    fPrefs.setUserEmailAddress(email);

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
    if (aValue instanceof UserPrefs) {
      UserPrefs oldPrefs = fPrefs;
      fPrefs = (UserPrefs) aValue;

      fValues.put(kNameKey, fPrefs.getUserName());
      fValues.put(kOrganizationKey, fPrefs.getUserOrganization());
      fValues.put(kEmailAddressKey, fPrefs.getUserEmailAddress());

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
