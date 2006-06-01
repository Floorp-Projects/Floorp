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


// This is our controller -- the thingy that listens to what the user
// is doing. There is one controller per browser window, and each has
// a BrowserView that manages information about problems within the
// window. The controller figures out when the browser might want to
// know about something, but the browser view figures out what exactly
// to do (and the BrowserView's displayer figures out how to do it).
//
// For example, the controller might notice that the user has switched
// to a tab that has something problematic in it. It would tell its 
// BrowserView this, and the BrowserView would figure out whether it 
// is appropriate to show a warning (e.g., perhaps the user previously
// dismissed the warning for that problem). If so, the BrowserView tells
// the displayer to show the warning. Anyhoo...
//
// TODO Could move all browser-related hide/show logic into the browser
//      view. Need to think about this more.

/**
 * Handles user actions, translating them into messages to the view
 *
 * @constructor
 * @param win Reference to the Window (browser window context) we should
 *            attach to
 * @param tabWatcher  Reference to the TabbedBrowserWatcher object 
 *                    the controller should use to receive events about tabs.
 * @param phishingWarden Reference to the PhishingWarden we should register
 *                       our browserview with
 */
function PROT_Controller(win, tabWatcher, phishingWarden) {
  this.debugZone = "controller";

  this.win_ = win;
  this.phishingWarden_ = phishingWarden;

  // Use this to query preferences
  this.prefs_ = new G_Preferences();

  // Set us up to receive the events we want.
  this.tabWatcher_ = tabWatcher;
  this.onTabSwitchCallback_ = BindToObject(this.onTabSwitch, this);
  this.tabWatcher_.registerListener("tabswitch",
                                    this.onTabSwitchCallback_);


  // Install our command controllers. These commands are issued from
  // various places in our UI, including our preferences dialog, the
  // warning dialog, etc.
  var commandHandlers = { 
    "safebrowsing-show-warning" :
      BindToObject(this.onUserShowWarning, this),
    "safebrowsing-accept-warning" :
      BindToObject(this.onUserAcceptWarning, this),
    "safebrowsing-decline-warning" :
      BindToObject(this.onUserDeclineWarning, this),
    "safebrowsing-submit-blacklist" :
      BindToObject(this.onUserSubmitToBlacklist, this),
    "safebrowsing-submit-generic-phishing" :
      BindToObject(this.onUserSubmitToGenericPhish, this),
    "safebrowsing-preferences" :
      BindToObject(this.onUserPreferences, this),
    "safebrowsing-test-link" :
      BindToObject(this.showURL_, this, PROT_GlobalStore.getTestURLs()[0]),
    "safebrowsing-preferences-home-link":
      BindToObject(this.showURL_, this, PROT_GlobalStore.getHomePageURL()),
    "safebrowsing-preferences-policy-link":
      BindToObject(this.showURL_, this, PROT_GlobalStore.getPolicyURL()),
    "safebrowsing-preferences-home-link-nochrome":
      BindToObject(this.showURL_, this, PROT_GlobalStore.getHomePageURL(), 
                   true /* chromeless */),
    "safebrowsing-preferences-policy-link-nochrome":
      BindToObject(this.showURL_, this, PROT_GlobalStore.getPolicyURL(), 
                   true /* chromeless */),
  };

  this.commandController_ = new PROT_CommandController(commandHandlers);
  this.win_.controllers.appendController(this.commandController_);

  // This guy embodies the logic of when to display warnings
  // (displayers embody the how).
  this.browserView_ = new PROT_BrowserView(this.tabWatcher_, 
                                           this.win_.document);

  // We need to let the phishing warden know about this browser view so it 
  // can be given the opportunity to handle problem documents. We also need
  // to let the warden know when this window and hence this browser view
  // is going away.
  this.phishingWarden_.addBrowserView(this.browserView_);

  this.windowWatcher_ = 
    Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
    .getService(Components.interfaces.nsIWindowWatcher);

  G_Debug(this, "Controller initialized.");
}

/**
 * Invoked when the browser window is closing. Do some cleanup.
 */
PROT_Controller.prototype.shutdown = function(e) {
  G_Debug(this, "Browser window closing. Shutting controller down.");
  if (this.browserView_) {
    this.phishingWarden_.removeBrowserView(this.browserView_);
  }

  if (this.commandController_) {
    this.win_.controllers.removeController(this.commandController_);
    this.commandController_ = null;
  }


  // No need to drain the browser view's problem queue explicitly; it will
  // receive pagehides for all the browsers in its queues as they're torn
  // down, and it will remove them.
  this.browserView_ = null;

  if (this.tabWatcher_) {
    this.tabWatcher_.removeListener("tabswitch", 
                                    this.onTabSwitchCallback_);
    this.tabWatcher_.shutdown();
  }

  this.win_.removeEventListener("unload", this.onShutdown_, false);
  this.prefs_ = null;

  this.windowWatcher_ = null;

  G_Debug(this, "Controller shut down.");
}

/**
 * The user clicked the urlbar icon; they want to see the warning message
 * again.
 */
PROT_Controller.prototype.onUserShowWarning = function() {
  var browser = this.tabWatcher_.getCurrentBrowser();
  this.browserView_.explicitShow(browser);
}

/**
 * Deal with a user wanting preferences
 */
PROT_Controller.prototype.onUserPreferences = function() {
  G_Debug(this, "User wants preferences.");
  var instantApply = this.prefs_.getPref("browser.preferences.instantApply", 
                                         false);
  var features = "chrome,titlebar,toolbar,centerscreen" + 
                 (instantApply ? ",dialog=no" : ",modal");
  var target = this.windowWatcher_.openWindow(
      this.win_,
      "chrome://safe-browsing/content/safebrowsing-preferences.xul",
      "safebrowsingprefsdialog",
      features,
      null /* args */);

  return true;
}

/**
 * The user clicked on one of the links in the preferences text.
 * Display the corresponding page in a new window with all the chrome
 * enabled.
 *
 * @param url The URL to display in a new window
 * @param opt_chromeless Boolean indicating whether to open chromeless
 */
PROT_Controller.prototype.showURL_ = function(url, opt_chromeless) {
  var features = opt_chromeless ? "status,scrollbars=yes,resizable=yes" : null;
  this.windowWatcher_.openWindow(this.win_,
                                 url,
                                 "_blank",
                                 features,
                                 null);
}

/**
 * User wants to report a phishing page.
 *
 * TODO: pass url as query param. This is ugly.
 */
PROT_Controller.prototype.onUserSubmitToBlacklist = function() {
  var current_window = this.tabWatcher_.getCurrentWindow();
  G_Debug(this, "User wants to submit to blacklist: " +
          current_window.location.href);

  var target = this.windowWatcher_.openWindow(
      this.windowWatcher_.activeWindow /* parent */,
      PROT_GlobalStore.getSubmitUrl(),
      "_blank",
      "height=400em,width=800,scrollbars=yes,resizable=yes," +
      "menubar,toolbar,location,directories,personalbar,status",
      null /* args */);

  this.maybeFillInURL_(current_window, target);
  return true;
}

/**
 * User wants to report something phishy, but we don't know if it's a 
 * false positive or negative.
 *
 * TODO: pass url as query param. This is ugly.
 */
PROT_Controller.prototype.onUserSubmitToGenericPhish = function() {
  var current_window = this.tabWatcher_.getCurrentWindow();
  G_Debug(this, "User wants to submit something about: " +
          current_window.location.href);

  var target = this.windowWatcher_.openWindow(
      this.windowWatcher_.activeWindow /* parent */,
      PROT_GlobalStore.getGenericPhishSubmitURL(),
      "_blank",
      "height=400em,width=800,scrollbars=yes,resizable=yes," +
      "menubar,toolbar,location,directories,personalbar,status",
      null /* args */);

  this.maybeFillInURL_(current_window, target);
  return true;
}

/**
 * A really lame method used by the submission report commands to fill
 * the current URL into the appropriate form field of submission page.
 *
 * TODO: this really needs an overhaul. 
 */
PROT_Controller.prototype.maybeFillInURL_ = function(current_window, target) {
  // TODO: merge in patch from perforce
  return true;
}

/**
 * Deal with a user accepting our warning. 
 *
 * TODO the warning hide/display instructions here can probably be moved
 * into the browserview in the future, given its knowledge of when the
 * problem doc hides/shows.
 */
PROT_Controller.prototype.onUserAcceptWarning = function() {
  G_Debug(this, "User accepted warning.");
  var browser = this.tabWatcher_.getCurrentBrowser();
  G_Assert(this, !!browser, "Couldn't get current browser?!?");
  G_Assert(this, this.browserView_.hasProblem(browser),
           "User accept fired, but browser doesn't have warning showing?!?");

  this.browserView_.acceptAction(browser);
  this.browserView_.problemResolved(browser);
}

/**
 * Deal with a user declining our warning. 
 *
 * TODO the warning hide/display instructions here can probably be moved
 * into the browserview in the future, given its knowledge of when the
 * problem doc hides/shows.
 */
PROT_Controller.prototype.onUserDeclineWarning = function() {
  G_Debug(this, "User declined warning.");
  var browser = this.tabWatcher_.getCurrentBrowser();
  G_Assert(this, this.browserView_.hasProblem(browser),
           "User decline fired, but browser doesn't have warning showing?!?");
  this.browserView_.declineAction(browser);
  // We don't call problemResolved() here because all declining does it
  // hide the message; we still have the urlbar icon showing, giving
  // the user the ability to bring the warning message back up if they
  // so desire.
}

/**
 * Notice tab switches, and display or hide warnings as appropriate.
 *
 * TODO this logic can probably move into the browser view at some
 * point. But one thing at a time.
 */
PROT_Controller.prototype.onTabSwitch = function(e) {
  if (this.browserView_.hasProblem(e.fromBrowser)) 
    this.browserView_.problemBrowserUnselected(e.fromBrowser);

  if (this.browserView_.hasProblem(e.toBrowser))
    this.browserView_.problemBrowserSelected(e.toBrowser);
}

/**
 * Load a URI in the browser
 *
 * @param browser Browser in which to load the URI
 * @param url URL to load
 */
PROT_Controller.prototype.loadURI = function(browser, url) {
  browser.loadURI(url, null, null);
}

/**
 * Check all browsers (tabs) to see if any of them are phishy.
 * This isn't that clean of a design because as new wardens get
 * added, this method needs to be updated manually.  TODO: fix this
 * when we add a second warden and know what the needs are.
 */
PROT_Controller.prototype.checkAllBrowsers = function() {
  var browsers = this.tabWatcher_.getTabBrowser().browsers;
  for (var i = 0, browser = null; browser = browsers[i]; ++i) {
    // Check window and all frames.
    this.checkAllHtmlWindows_(browser, browser.contentWindow)
  }
}

/**
 * Check the HTML window and all containing frames for phishing urls.
 * @param browser ChromeWindow that contains the html window
 * @param win HTMLWindow
 */
PROT_Controller.prototype.checkAllHtmlWindows_ = function(browser, win) {
  // Check this window
  var doc = win && win.document;
  if (!doc)
    return;

  var url = doc.location.href;
  
  var callback = BindToObject(this.browserView_.isProblemDocument_,
                              this.browserView_,
                              browser,
                              doc,
                              this.phishingWarden_);

  this.phishingWarden_.checkUrl_(url, callback);
  
  // Check all frames
  for (var i = 0, frame = null; frame = win.frames[i]; ++i) {
    this.checkAllHtmlWindows_(browser, frame);
  }
}
