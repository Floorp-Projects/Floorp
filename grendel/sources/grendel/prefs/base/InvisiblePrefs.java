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

import java.awt.Rectangle;

public class InvisiblePrefs {

  private static InvisiblePrefs MasterInvisiblePrefs;
  
  public static synchronized InvisiblePrefs GetMaster() {
    if (MasterInvisiblePrefs == null) {
      MasterInvisiblePrefs = new InvisiblePrefs();
    }
    return MasterInvisiblePrefs;
  }

  Preferences prefs;
  
  private InvisiblePrefs() {
    prefs = PreferencesFactory.Get();
    readPrefs();
  }

  public void readPrefs() {
    setFolderPanelColumnLayout(prefs.getString ("invisible.column.folderpanel",""));
    setMasterPanelColumnLayout(prefs.getString ("invisible.column.masterpanel",""));
    writePrefs();
  }
  
  public void writePrefs() {
    prefs.putString ("invisible.column.folderpanel",getFolderPanelColumnLayout());
    prefs.putString ("invisible.column.masterpanel",getMasterPanelColumnLayout());
  }
  
  String myFolderPanelColumnLayout;
  String myMasterPanelColumnLayout;
  
  public String getFolderPanelColumnLayout() {
    return myFolderPanelColumnLayout;
  }
  
  public String getMasterPanelColumnLayout() {
    return myMasterPanelColumnLayout;
  }
  
  public void setFolderPanelColumnLayout(String aFolderPanelColumnLayout) {
    myFolderPanelColumnLayout = aFolderPanelColumnLayout;
  }
  
  public void setMasterPanelColumnLayout(String aMasterPanelColumnLayout) {
    myMasterPanelColumnLayout = aMasterPanelColumnLayout;
  }
  
  public void setBounds(String aName, Rectangle b) {
    prefs.putInt("invisible.framebounds."+aName+".x",b.x);
    prefs.putInt("invisible.framebounds."+aName+".y",b.y);
    prefs.putInt("invisible.framebounds."+aName+".width",b.width);
    prefs.putInt("invisible.framebounds."+aName+".height",b.height);
  }
  
  public Rectangle getBounds(String aName, int aWidth, int aHeight) {
    int x = prefs.getInt("invisible.framebounds."+aName+".x",100);
    int y = prefs.getInt("invisible.framebounds."+aName+".y",100);
    int w = prefs.getInt("invisible.framebounds."+aName+".width",aWidth);
    int h = prefs.getInt("invisible.framebounds."+aName+".height",aHeight);
    return new Rectangle(x,y,w,h); 
  }
  
  public void setMultiPaneSizes(int fx, int fy, int tx, int ty) {
    prefs.putInt("invisible.multipane.folder.x",fx);
    prefs.putInt("invisible.multipane.folder.y",fy);
    prefs.putInt("invisible.multipane.thread.x",tx);
    prefs.putInt("invisible.multipane.thread.y",ty);
  }
  
  public int getMultiPaneFolderX(int aDefault) {
    return prefs.getInt("invisible.multipane.folder.x", aDefault);
  }

  public int getMultiPaneFolderY(int aDefault) {
    return prefs.getInt("invisible.multipane.folder.y", aDefault);
  }

  public int getMultiPaneThreadX(int aDefault) {
    return prefs.getInt("invisible.multipane.thread.x", aDefault);
  }

  public int getMultiPaneThreadY(int aDefault) {
    return prefs.getInt("invisible.multipane.thread.y", aDefault);
  }

}
