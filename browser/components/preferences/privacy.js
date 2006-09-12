/*
# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Jeff Walden <jwalden+code@mit.edu>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

var gPrivacyPane = {

  /**
   * Sets up the UI for the number of days of history to keep, and updates the
   * label of the "Clear Now..." button.
   */
  init: function ()
  {
    this._updateHistoryDaysUI();
    this.updateClearNowButtonLabel();
  },

  // HISTORY

  /*
   * Preferences:
   *
   * browser.history_expire_days
   * - the number of days of history to remember
   * browser.history_expire_days.mirror
   * - a preference whose value mirrors that of browser.history_expire_days, to
   *   make the "days of history" checkbox easier to code
   * browser.formfill.enable
   * - true if entries in forms and the search bar should be saved, false
   *   otherwise
   * browser.download.manager.retention
   * - determines when downloads are automatically removed from the download
   *   manager:
   *
   *     0 means remove downloads when they finish downloading
   *     1 means downloads will be removed when the browser quits
   *     2 means never remove downloads
   */

  /**
   * Initializes the days-of-history mirror preference and connects it to the
   * days-of-history checkbox so that updates to the textbox are transmitted to
   * the real days-of-history preference.
   */
  _updateHistoryDaysUI: function ()
  {
    var pref = document.getElementById("browser.history_expire_days");
    var mirror = document.getElementById("browser.history_expire_days.mirror");
    var textbox = document.getElementById("historyDays");
    var checkbox = document.getElementById("rememberHistoryDays");

    // handle mirror non-existence or mirror/pref unsync
    if (mirror.value === null || mirror.value != pref.value)
      mirror.value = pref.value ? pref.value : pref.defaultValue;

    checkbox.checked = (pref.value > 0);
    textbox.disabled = !checkbox.checked;

    // hook up textbox to mirror preference and force a preference read
    textbox.setAttribute("onsynctopreference", "return gPrivacyPane._writeHistoryDaysMirror();");
    textbox.setAttribute("preference", "browser.history_expire_days.mirror");
    mirror.updateElements();
  },

  /**
   * Stores the days of history to the actual days-of-history preference and
   * returns that value, to be stored in the mirror preference.
   */
  _writeHistoryDaysMirror: function ()
  {
    var pref = document.getElementById("browser.history_expire_days");
    var textbox = document.getElementById("historyDays");
    pref.value = textbox.value;

    // don't override the value in the textbox
    return undefined;
  },

  /**
   * Responds to the checking or unchecking of the days-of-history UI, storing
   * the appropriate value to the days-of-history preference and enabling or
   * disabling the number textbox as appropriate.
   */
  onchangeHistoryDaysCheck: function ()
  {
    var pref = document.getElementById("browser.history_expire_days");
    var mirror = document.getElementById("browser.history_expire_days.mirror");
    var textbox = document.getElementById("historyDays");
    var checkbox = document.getElementById("rememberHistoryDays");

    pref.value = checkbox.checked ? mirror.value : 0;
    textbox.disabled = !checkbox.checked;
  },

  /**
   * Converts the value of the browser.download.manager.retention preference
   * into a Boolean value.  "remove on close" and "don't remember" both map
   * to an unchecked checkbox, while "remember" maps to a checked checkbox.
   */
  readDownloadRetention: function ()
  {
    var pref = document.getElementById("browser.download.manager.retention");
    return (pref.value == 2);
  },

  /**
   * Returns the appropriate value of the browser.download.manager.retention
   * preference for the current UI.
   */
  writeDownloadRetention: function ()
  {
    var checkbox = document.getElementById("rememberDownloads");
    return checkbox.checked ? 2 : 0;
  },

  // COOKIES

  /*
   * Preferences:
   *
   * network.cookie.cookieBehavior
   * - determines how the browser should handle cookies:
   *     0   means enable all cookies
   *     1   means allow cookies from the "originating" server only; see
   *         netwerk/cookie/src/nsCookieService.cpp for a hairier definition
   *     2   means disable all cookies
   *     3   means use P3P policy to decide, which is probably broken
   * network.cookie.lifetimePolicy
   * - determines how long cookies are stored:
   *     0   means keep cookies until they expire
   *     1   means ask how long to keep each cookie
   *     2   means keep cookies until the browser is closed
   */

  /**
   * Reads the network.cookie.cookieBehavior preference value and
   * enables/disables the "Keep until:" UI accordingly, returning true
   * if cookies are enabled.
   */
  readAcceptCookies: function ()
  {
    var pref = document.getElementById("network.cookie.cookieBehavior");
    var keepUntil = document.getElementById("keepUntil");
    var menu = document.getElementById("keepCookiesUntil");

    // anything other than "disable all cookies" maps to "accept cookies"
    var acceptCookies = (pref.value != 2);

    keepUntil.disabled = menu.disabled = !acceptCookies;
    
    return acceptCookies;
  },

  /**
   * Enables/disables the "keep until" label and menulist in response to the
   * "accept cookies" checkbox being checked or unchecked.
   */
  writeAcceptCookies: function ()
  {
    var checkbox = document.getElementById("acceptCookies");
    return checkbox.checked ? 0 : 2;
  },

  /**
   * Displays fine-grained, per-site preferences for cookies.
   */
  showCookieExceptions: function ()
  {
    var bundlePreferences = document.getElementById("bundlePreferences");
    var params = { blockVisible   : true, 
                   sessionVisible : true, 
                   allowVisible   : true, 
                   prefilledHost  : "", 
                   permissionType : "cookie",
                   windowTitle    : bundlePreferences.getString("cookiepermissionstitle"),
                   introText      : bundlePreferences.getString("cookiepermissionstext") };
    document.documentElement.openWindow("Browser:Permissions",
                                        "chrome://browser/content/preferences/permissions.xul",
                                        "", params);
  },

  /**
   * Displays all the user's cookies in a dialog.
   */  
  showCookies: function (aCategory)
  {
    document.documentElement.openWindow("Browser:Cookies",
                                        "chrome://browser/content/preferences/cookies.xul",
                                        "", null);
  },

  // CLEAR PRIVATE DATA

  /*
   * Preferences:
   *
   * privacy.sanitize.sanitizeOnShutdown
   * - true if the user's private data is cleared on startup according to the
   *   Clear Private Data settings, false otherwise
   * privacy.sanitize.promptOnSanitize
   * - true if sanitizing forces the user to accept a dialog, false otherwise
   */

  /**
   * Sets the label of the "Clear Now..." button according to the
   * privacy.sanitize.promptOnSanitize preference. Read valueFromPreferences to
   * only change the button when the underlying pref changes, since in the case
   * of instantApply=false, the call to clearPrivateDataNow would result in the
   * dialog appearing when the user just unchecked the "Ask me" checkbox.
   */
  updateClearNowButtonLabel: function ()
  {
    var pref = document.getElementById("privacy.sanitize.promptOnSanitize");
    var clearNowButton = document.getElementById("clearDataNow");

    if (pref.valueFromPreferences)
      clearNowButton.label = clearNowButton.getAttribute("label1"); // "Clear Now..."
    else
      clearNowButton.label = clearNowButton.getAttribute("label2"); // "Clear Now"
  },

  /**
   * Displays the Clear Private Data settings dialog.
   */
  showClearPrivateDataSettings: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/sanitize.xul",
                                           "", null);
    this.updateClearNowButtonLabel();
  },

  /**
   * Either displays a dialog from which individual parts of private data may be
   * cleared, or automatically clears private data according to current
   * CPD settings.  The former happens if privacy.sanitize.promptOnSanitize is
   * true, and the latter happens otherwise.
   */
  clearPrivateDataNow: function ()
  {
    const Cc = Components.classes, Ci = Components.interfaces;
    var glue = Cc["@mozilla.org/browser/browserglue;1"]
                 .getService(Ci.nsIBrowserGlue);
    glue.sanitize(window || null);
  }

};
