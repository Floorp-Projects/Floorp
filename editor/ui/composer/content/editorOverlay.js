/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

function EditorNew()
{
  dump("In EditorNew..\n");
  editorShell.NewWindow();
}

function EditorNewFromTemplate()
{
  dump("In EditorNewFromTemplate..\n");
}

function EditorNewFromDraft()
{
  dump("In EditorNewFromDraft..\n");
}

// copied from tasksOverlay
function EditorOpenBrowserWindow()
{
  pref = Components.classes['component://netscape/preferences'];

  // if all else fails, use trusty "about:blank" as the start page
  var startpage = "about:blank";  
  if (pref) {
    pref = pref.getService(); 
    pref = pref.QueryInterface(Components.interfaces.nsIPref);
  }

  if (pref) {
    // from mozilla/modules/libpref/src/init/all.js
    // 0 = blank 
    // 1 = home (browser.startup.homepage)
    // 2 = last 
    choice = 1;
    try {
                choice = pref.GetIntPref("browser.startup.page");
    }
    catch (ex) {
	    dump("failed to get the browser.startup.page pref\n");
    }
	
    switch (choice) {
      case 0:
        startpage = "about:blank";
        break;
      case 1:
        try {
                      startpage = pref.CopyCharPref("browser.startup.homepage");
        }
        catch (ex) {
	        dump("failed to get the browser.startup.homepage pref\n");
	        startpage = "about:blank";
        }
        break;
      case 2:
        try {
	        var history = Components.classes["component://netscape/browser/global-history"].getService();
                        history = history.QueryInterface(Components.interfaces.nsIGlobalHistory);
	        startpage = history.GetLastPageVisted();
        }
        catch (ex) {
	        dump(ex +"\n");
        }
        break;
      default:
        startpage = "about:blank";
    }
  }
//	window.open(startpage); // This doesn't size the window properly.
   window.openDialog( "chrome://navigator/content/navigator.xul", "_blank", "chrome,all,dialog=no", startpage );

}

