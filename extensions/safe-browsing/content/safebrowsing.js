/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// The glue file. Here we check to see if we should be active, and if
// so, apply our overlay and call into the application to attach a new
// controller to this window.  If not, this code is a no-op.
//
// This code is loaded into the global context by
// safebrowsing-overlay-bootstrapper.xul. 


window.addEventListener("load", SB_startup, false);

var SB_controller;  // Exported so it's accessible by child windows
var SB_application; // Our Application instance
var SB_appContext;  // The context in which our Application lives

function SB_startup() {

  // Get a handle on our application context.

  var Cc = Components.classes;
  SB_appContext = Cc["@mozilla.org/safebrowsing/application;1"]
                      .getService();
  SB_appContext = SB_appContext.wrappedJSObject.appContext;

  // Now bail if we shouldn't be running in this Firefox. See application.js.

  if (!SB_appContext.PROT_Application.isCompatibleWithThisFirefox())
    return;

  // Yay! We should run! Now apply our overlay already. (See
  // safebrowsing-overlay-bootstrapper.xul).

  SB_loadOverlay();

  // Each new browser window needs its own controller. 

  SB_application = SB_appContext.PROT_application;

  var contentArea = document.getElementById("content");
  var tabWatcher = new SB_appContext.G_TabbedBrowserWatcher(
      contentArea, 
      "safebrowsing-watcher",
      true /*ignore about:blank*/);

  SB_controller = new SB_appContext.PROT_Controller(
      window, 
      tabWatcher, 
      SB_appContext.PROT_listManager, 
      SB_appContext.PROT_phishingWarden);
}


// Some utils for our UI.

/**
 * Execute a command on a window
 *
 * @param cmd String containing command to execute
 * @param win Reference to Window on which to execute it
 */
function SB_executeCommand(cmd, win) {
  try {	
    var disp = win.document.commandDispatcher;
    var ctrl = disp.getControllerForCommand(cmd);
    ctrl.doCommand(cmd);
  } catch (e) {
    dump("Exception on command: " + cmd + "\n");
    dump(e);
  }
}

/**
 * Execute a command on this window
 *
 * @param cmd String containing command to execute
 */
function SB_executeCommandLocally(cmd) {
  SB_executeCommand(cmd, window);
}

/**
 * Set status text for a particular link. We look the URLs up in our 
 * globalstore.
 *
 * @param link ID of a link for which we should show status text
 */
function SB_setStatusFor(link) {
  var gs = SB_appContext.PROT_globalStore;
  var msg;
  if (link == "safebrowsing-palm-faq-link")
    msg = gs.getPhishingFaqURL(); 
  else if (link == "safebrowsing-palm-phishingorg-link")
    msg = gs.getAntiPhishingURL(); 
  else if (link == "safebrowsing-palm-fraudpage-link")
    msg = gs.getHomePageURL();
  else if (link == "safebrowsing-palm-falsepositive-link")
    msg = gs.getFalsePositiveURL();
  else if (link == "safebrowsing-palm-report-link")
    msg = gs.getSubmitUrl();
  else
    msg = "";
  
  SB_setStatus(msg);
}

/**
 * Actually display the status text
 *
 * @param msg String that we should show in the statusbar
 */
function SB_setStatus(msg) {
  document.getElementById("statusbar-display").label = msg;
}

/**
 * Clear the status text
 */
function SB_clearStatus() {
  document.getElementById("statusbar-display").label = "";
}


/**
 * Dynamically load our overlay onto this document (browser.xul).
 */ 
function SB_loadOverlay() {

  // We can't hide the urlbar icon until the overlay has actually loaded.
  var observer = 
    new SB_appContext.G_ObserverWrapper("xul-overlay-merged" /*topic*/, 
                                        SB_fixupUrlbarIcon);

  document.loadOverlay(
      "chrome://safe-browsing/content/safebrowsing-overlay.xul", 
      observer);
}

/**
 * We manually move our urlbar icon into the urlbar when the browser
 * loads, and then make it invisible. We do this because in pre-2.0
 * there is no id on the parent of the lock icon, so there's no way to
 * statically overlay next to it. So we do it dynamically.
 */
function SB_fixupUrlbarIcon() {
  var urlbarIcon = document.getElementById("safebrowsing-urlbar-icon");
  if (!urlbarIcon)
    return;
  urlbarIcon.parentNode.removeChild(urlbarIcon);
  
  var lock = document.getElementById("lock-icon");
  if (!lock) 
    return;
  lock.parentNode.appendChild(urlbarIcon);
  
  urlbarIcon.style.visibility = "collapse";
}
