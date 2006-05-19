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


/**
 * This file is included into the main browser chrome from
 * browser/base/content/global-scripts.inc
 */

var safebrowsing = {
  controller: null,
  globalStore: null,
  phishWarden: null,

  startup: function() {
    setTimeout(safebrowsing.deferredStartup, 2000);

    // clean up
    window.removeEventListener("load", safebrowsing.startup, false);
  },
  
  deferredStartup: function() {
    var Cc = Components.classes;
    var appContext = Cc["@mozilla.org/safebrowsing/application;1"]
                     .getService().wrappedJSObject;
    safebrowsing.globalStore = appContext.PROT_GlobalStore;

    // Each new browser window needs its own controller. 

    var contentArea = document.getElementById("content");

    var phishWarden = new appContext.PROT_PhishingWarden();
    safebrowsing.phishWarden = phishWarden;

    // Register tables
    // XXX: move table names to a pref that we originally will download
    // from the provider (need to workout protocol details)
    phishWarden.registerWhiteTable("goog-white-domain");
    phishWarden.registerWhiteTable("goog-white-url");
    phishWarden.registerBlackTable("goog-black-url");

    // Download/update lists if we're in non-enhanced mode
    phishWarden.maybeToggleUpdateChecking();
    var tabWatcher = new appContext.G_TabbedBrowserWatcher(
        contentArea,
        "safebrowsing-watcher",
        true /*ignore about:blank*/);
    safebrowsing.controller = new appContext.PROT_Controller(
        window,
        tabWatcher,
        phishWarden);
    
    // The initial pages may be a phishing site (e.g., user clicks on a link
    // in an email message and it opens a new window with a phishing site),
    // so we need to check all open tabs.
    safebrowsing.controller.checkAllBrowsers();
  },

  /**
   * Clean up.
   */
  shutdown: function() {
    if (safebrowsing.controller) {
      // If the user shuts down before deferredStartup, there is no controller.
      safebrowsing.controller.shutdown();
    }
    if (safebrowsing.phishWarden) {
      safebrowsing.phishWarden.shutdown();
    }
    
    window.removeEventListener("unload", safebrowsing.shutdown, false);
  }
}

window.addEventListener("load", safebrowsing.startup, false);
window.addEventListener("unload", safebrowsing.shutdown, false);


// XXX Everything below here should be removed from the global namespace and
// moved into the safebrowsing object.

// Some utils for our UI.

/**
 * Set status text for a particular link. We look the URLs up in our 
 * globalstore.
 *
 * @param link ID of a link for which we should show status text
 */
function SB_setStatusFor(link) {
  var gs = safebrowsing.globalStore;
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
