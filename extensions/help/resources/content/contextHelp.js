/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   R.J. Keller <rlk@trfenv.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const MOZILLA_CONTENT_PACK = "chrome://help/locale/mozillahelp.rdf";
//Set the default content pack to the Mozilla content pack. Use the
//setHelpFileURI function to set this value.
var helpFileURI = MOZILLA_CONTENT_PACK;

// openHelp - Opens up the Mozilla Help Viewer with the specified
//    topic and content pack.
// see http://www.mozilla.org/projects/help-viewer/content_packs.html
function openHelp(topic, contentPack)
{
  //cp is the content pack to use in this function. If the contentPack
  //parameter was set, we will use that content pack. If not, we will
  //use the default content pack set by setHelpFileURI().
  var cp = contentPack || helpFileURI;

  // Try to find previously opened help.
  var topWindow = locateHelpWindow(cp);

  if ( topWindow ) {
    // Open topic in existing window.
    topWindow.focus();
    topWindow.displayTopic(topic);
  } else {
    // Open topic in new window.
    const params = Components.classes["@mozilla.org/embedcomp/dialogparam;1"]
                             .createInstance(Components.interfaces.nsIDialogParamBlock);
    params.SetNumberStrings(2);
    params.SetString(0, cp);
    params.SetString(1, topic);
    const ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(Components.interfaces.nsIWindowWatcher);
    ww.openWindow(null, "chrome://help/content/help.xul", "_blank", "chrome,all,alwaysRaised,dialog=no", params);
  }
}

//setHelpFileURI - Sets the default content pack to use in the Help Viewer
function setHelpFileURI(rdfURI) {
  helpFileURI = rdfURI; 
}

// Locate mozilla:help window (if any) opened for this help file uri.
function locateHelpWindow(helpFileURI) {
  const windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1']
                                  .getService(Components.interfaces.nsIWindowMediator);
  var iterator = windowManager.getEnumerator("mozilla:help");
  var topWindow = null;
  var currentWindow;

  // Loop through the help windows looking for one with the
  // current Help Content Pack loaded.
  while (iterator.hasMoreElements()) {
    currentWindow = iterator.getNext();
    if (currentWindow.getHelpFileURI() == helpFileURI) {
      topWindow = currentWindow;
      break;  
    }  
  }
  return topWindow;
}
