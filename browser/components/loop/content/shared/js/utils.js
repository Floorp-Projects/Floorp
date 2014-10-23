/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.utils = (function(mozL10n) {
  "use strict";

  /**
   * Call types used for determining if a call is audio/video or audio-only.
   */
  var CALL_TYPES = {
    AUDIO_VIDEO: "audio-video",
    AUDIO_ONLY: "audio"
  };

  /**
   * Format a given date into an l10n-friendly string.
   *
   * @param {Integer} The timestamp in seconds to format.
   * @return {String} The formatted string.
   */
  function formatDate(timestamp) {
    var date = (new Date(timestamp * 1000));
    var options = {year: "numeric", month: "long", day: "numeric"};
    return date.toLocaleDateString(navigator.language, options);
  }

  /**
   * Used for getting a boolean preference. It will either use the browser preferences
   * (if navigator.mozLoop is defined) or try to get them from localStorage.
   *
   * @param {String} prefName The name of the preference. Note that mozLoop adds
   *                          'loop.' to the start of the string.
   *
   * @return The value of the preference, or false if not available.
   */
  function getBoolPreference(prefName) {
    if (navigator.mozLoop) {
      return !!navigator.mozLoop.getLoopBoolPref(prefName);
    }

    return !!localStorage.getItem(prefName);
  }

  /**
   * Helper for general things
   */
  function Helper() {
    this._iOSRegex = /^(iPad|iPhone|iPod)/;
  }

  Helper.prototype = {
    isFirefox: function(platform) {
      return platform.indexOf("Firefox") !== -1;
    },

    isFirefoxOS: function(platform) {
      // So far WebActivities are exposed only in FxOS, but they may be
      // exposed in Firefox Desktop soon, so we check for its existence
      // and also check if the UA belongs to a mobile platform.
      // XXX WebActivities are also exposed in WebRT on Firefox for Android,
      //     so we need a better check. Bug 1065403.
      return !!window.MozActivity && /mobi/i.test(platform);
    },

    isIOS: function(platform) {
      return this._iOSRegex.test(platform);
    },

    /**
     * Helper to allow getting some of the location data in a way that's compatible
     * with stubbing for unit tests.
     */
    locationData: function() {
      return {
        hash: window.location.hash,
        pathname: window.location.pathname
      };
    }
  };

  /**
   * Generates and opens a mailto: url with call URL information prefilled.
   * Note: This only works for Desktop.
   *
   * @param  {String} callUrl   The call URL.
   * @param  {String} recipient The recipient email address (optional).
   */
  function composeCallUrlEmail(callUrl, recipient) {
    if (typeof navigator.mozLoop === "undefined") {
      console.warn("composeCallUrlEmail isn't available for Loop standalone.");
      return;
    }
    navigator.mozLoop.composeEmail(
      mozL10n.get("share_email_subject4", {
        clientShortname: mozL10n.get("clientShortname2")
      }),
      mozL10n.get("share_email_body4", {
        callUrl: callUrl,
        clientShortname: mozL10n.get("clientShortname2"),
        learnMoreUrl: navigator.mozLoop.getLoopCharPref("learnMoreUrl")
      }),
      recipient
    );
  }

  return {
    CALL_TYPES: CALL_TYPES,
    Helper: Helper,
    composeCallUrlEmail: composeCallUrlEmail,
    formatDate: formatDate,
    getBoolPreference: getBoolPreference
  };
})(document.mozL10n || navigator.mozL10n);
