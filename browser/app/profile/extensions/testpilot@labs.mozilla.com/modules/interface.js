/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Choose correct overlay to apply based on user's update channel setting;
 * do any other tweaking to UI needed to work correctly with user's version.
 * 1. Fx 3.*, default update channel -> TP icon menu in status bar
 * 2. beta update channel -> Feedback button in toolbar, customizable
 * 3. Fx 4.*, default update channel -> TP icon menu in add-on bar
 */

// A lot of the stuff that's currently in browser.js can get moved here.

EXPORTED_SYMBOLS = ["TestPilotUIBuilder"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const UPDATE_CHANNEL_PREF = "app.update.channel";
const POPUP_SHOW_ON_NEW = "extensions.testpilot.popup.showOnNewStudy";
const POPUP_CHECK_INTERVAL = "extensions.testpilot.popup.delayAfterStartup";

var TestPilotUIBuilder = {
  get _prefs() {
    delete this._prefs;
    return this._prefs = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
  },

  get _prefDefaultBranch() {
    delete this._prefDefaultBranch;
    return this._prefDefaultBranch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefService).getDefaultBranch("");
  },

  get _comparator() {
    delete this._comparator;
    return this._comparator = Cc["@mozilla.org/xpcom/version-comparator;1"]
      .getService(Ci.nsIVersionComparator);
  },

  get _appVersion() {
    delete this._appVersion;
    return this._appVersion = Cc["@mozilla.org/xre/app-info;1"]
      .getService(Ci.nsIXULAppInfo).version;
  },

  buildTestPilotInterface: function(window) {
    // Don't need Feedback button: remove it
    let feedbackButton = window.document.getElementById("feedback-menu-button");
    if (!feedbackButton) {
      let toolbox = window.document.getElementById("navigator-toolbox");
      let palette = toolbox.palette;
      feedbackButton = palette.getElementsByAttribute("id", "feedback-menu-button").item(0);
    }
    feedbackButton.parentNode.removeChild(feedbackButton);

    /* Default prefs for test pilot version - default to NOT notifying user about new
     * studies starting. Note we're setting default values, not current values -- we
     * want these to be overridden by any user set values!!*/
    this._prefDefaultBranch.setBoolPref(POPUP_SHOW_ON_NEW, false);
    this._prefDefaultBranch.setIntPref(POPUP_CHECK_INTERVAL, 180000);
  },

  buildFeedbackInterface: function(window) {
    /* If this is first run, and it's ffx4 beta version, and the feedback
     * button is not in the expected place, put it there!
     * (copied from MozReporterButtons extension) */

    /* Check if we've already done this customization -- if not, don't do it
     * again  (don't want to put it back in after user explicitly takes it out-
     * bug 577243 )*/
    let firefoxnav = window.document.getElementById("nav-bar");
    let pref = "extensions.testpilot.alreadyCustomizedToolbar";
    let alreadyCustomized = this._prefs.getBoolPref(pref);
    let curSet = firefoxnav.currentSet;

    if (!alreadyCustomized && (-1 == curSet.indexOf("feedback-menu-button"))) {
      // place the buttons after the search box.
      let newSet = curSet + ",feedback-menu-button";
      firefoxnav.setAttribute("currentset", newSet);
      firefoxnav.currentSet = newSet;
      window.document.persist("nav-bar", "currentset");
      this._prefs.setBoolPref(pref, true);
      // if you don't do the following call, funny things happen.
      try {
        window.BrowserToolboxCustomizeDone(true);
      } catch (e) {
      }
    }

    /* Pref defaults for Feedback version: default to notifying user about new
     * studies starting. Note we're setting default values, not current values -- we
     * want these to be overridden by any user set values!!*/
    this._prefDefaultBranch.setBoolPref(POPUP_SHOW_ON_NEW, true);
    this._prefDefaultBranch.setIntPref(POPUP_CHECK_INTERVAL, 600000);
  },

  channelUsesFeedback: function() {
    // Beta and aurora channels use feedback interface; nightly and release channels don't.
    let channel = this._prefDefaultBranch.getCharPref(UPDATE_CHANNEL_PREF);
    return (channel == "beta") || (channel == "betatest") || (channel == "aurora");
  },

  appVersionIsFinal: function() {
    // Return true iff app version >= 4.0 AND there is no "beta" or "rc" in version string.
    if (this._comparator.compare(this._appVersion, "4.0") >= 0) {
      if (this._appVersion.indexOf("b") == -1 && this._appVersion.indexOf("rc") == -1) {
        return true;
      }
    }
    return false;
  },

  buildCorrectInterface: function(window) {
    let firefoxnav = window.document.getElementById("nav-bar");
    /* This is sometimes called for windows that don't have a navbar - in
     * that case, do nothing. TODO maybe this should be in onWindowLoad?*/
    if (!firefoxnav) {
      return;
    }

    /* Overlay Feedback XUL if we're in the beta update channel, Test Pilot XUL otherwise, and
     * call buildFeedbackInterface() or buildTestPilotInterface(). */
    if (this.channelUsesFeedback()) {
      window.document.loadOverlay("chrome://testpilot/content/feedback-browser.xul", null);
      this.buildFeedbackInterface(window);
    } else {
      window.document.loadOverlay("chrome://testpilot/content/tp-browser.xul", null);
      this.buildTestPilotInterface(window);
    }
  }
};