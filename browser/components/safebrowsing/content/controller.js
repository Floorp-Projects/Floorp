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
