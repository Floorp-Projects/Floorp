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
 * The Initial Developer of the Original Code is Edwin Woudt 
 * <edwin@woudt.nl>.  Portions created by Edwin Woudt are
 * Copyright (C) 1999 Edwin Woudt. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package grendel.prefs.base;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import grendel.ui.UnifiedMessageDisplayManager;

public class UIPrefs {

  private static UIPrefs MasterUIPrefs;
  
  public static synchronized UIPrefs GetMaster() {
    if (MasterUIPrefs == null) {
      MasterUIPrefs = new UIPrefs();
    }
    return MasterUIPrefs;
  }

  Preferences prefs;
  
  private UIPrefs() {
    prefs = PreferencesFactory.Get();
    readPrefs();
  }

  public void readPrefs() {
    setDisplayManager (prefs.getString ("ui.displaymanager","unified"));
    setTooltips       (prefs.getBoolean("ui.tooltips",true));
    setMultiPaneLayout(prefs.getString ("ui.multipanelayout",UnifiedMessageDisplayManager.SPLIT_RIGHT));
    setLookAndFeel    (prefs.getString ("ui.lookandfeel","The Java(tm) Look and Feel"));
    writePrefs();
  }
  
  public void writePrefs() {
    prefs.putString ("ui.displaymanager" ,getDisplayManager());
    prefs.putBoolean("ui.tooltips"       ,getTooltips());
    prefs.putString ("ui.multipanelayout",getMultiPaneLayout());
    prefs.putString ("ui.lookandfeel"    ,getLookAndFeel());
 }
  
  String myDisplayManager;
  boolean myTooltips;
  String myMultiPaneLayout;
  String myLookAndFeel;
  
  public String getDisplayManager() {
    return myDisplayManager;
  }
  
  public boolean getTooltips() {
    return myTooltips;
  }
  
  public String getMultiPaneLayout() {
    return myMultiPaneLayout;
  }
  
  public String getLookAndFeel() {
    return myLookAndFeel;
  }
  
  public void setDisplayManager(String aDisplayManager) {
    myDisplayManager = aDisplayManager;
  }
  
  public void setTooltips(boolean aTooltips) {
    myTooltips = aTooltips;
  }
  
  public void setMultiPaneLayout(String aMultiPaneLayout) {
    myMultiPaneLayout = aMultiPaneLayout;
  }
  
  public void setLookAndFeel(String aLookAndFeel) {
    myLookAndFeel = aLookAndFeel;
  }
  
}

  