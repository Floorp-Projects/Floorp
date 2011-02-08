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
 * The Original Code is Test Pilot.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jono X <jono@mozilla.com>
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
const UPDATE_CHANNEL_PREF = "app.update.channel";

var TestPilotUIBuilder = {
  __prefs: null,
  get _prefs() {
    this.__prefs = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    return this.__prefs;
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
  },

  isBetaChannel: function() {
    // Nightly channel is treated the same as default channel.
    return (this._prefs.getCharPref(UPDATE_CHANNEL_PREF) == "beta");
  },

  buildCorrectInterface: function(window) {
    let firefoxnav = window.document.getElementById("nav-bar");
    /* This is sometimes called for windows that don't have a navbar - in
     * that case, do nothing. TODO maybe this should be in onWindowLoad?*/
    if (!firefoxnav) {
      return;
    }

    /* Overlay Feedback XUL if we're in the beta update channel, Test Pilot XUL otherwise.
     * Once the overlay is complete, call buildFeedbackInterface() or buildTestPilotInterface(). */
    if (this.isBetaChannel()) {
      let self = this;
      window.document.loadOverlay("chrome://testpilot/content/feedback-browser.xul",
                                  {observe: function(subject, topic, data) {
                                     if (topic == "xul-overlay-merged") {
                                       self.buildFeedbackInterface(window);
                                     }
                                   }});
    } else {
      window.document.loadOverlay("chrome://testpilot/content/tp-browser.xul",
                                  {observe: function(subject, topic, data) {
                                     if (topic == "xul-overlay-merged") {
                                       self.buildTestPilotInterface(window);
                                     }
                                  }});
    }
  }
};