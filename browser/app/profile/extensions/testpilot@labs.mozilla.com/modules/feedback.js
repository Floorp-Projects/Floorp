/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

EXPORTED_SYMBOLS = ["FeedbackManager"];

const Cc = Components.classes;
const Ci = Components.interfaces;

let Application = Cc["@mozilla.org/fuel/application;1"]
                  .getService(Ci.fuelIApplication);

var FeedbackManager = {
  _lastVisitedUrl: null,

  _happyUrl: null,
  get happyUrl() {
    if (!this._happyUrl) {
      this._happyUrl = Application.prefs.getValue("extensions.input.happyURL", "");
    }
    return this._happyUrl;
  },

  _sadUrl: null,
  get sadUrl() {
    if (!this._sadUrl) {
      this._sadUrl = Application.prefs.getValue("extensions.input.sadURL", "");
    }
    return this._sadUrl;
  },

  _brokenUrl: null,
  get brokenUrl() {
    if (!this._brokenUrl) {
      this._brokenUrl = Application.prefs.getValue("extensions.input.brokenURL", "");
    }
    return this._brokenUrl;
  },

  _ideaUrl: null,
  get ideaUrl() {
    if (!this._ideaUrl) {
      this._ideaUrl = Application.prefs.getValue("extensions.input.ideaURL", "");
    }
    return this._ideaUrl;
  },

  getFeedbackUrl: function FeedbackManager_getFeedbackUrl(menuItemChosen) {
    switch(menuItemChosen) {
    case "happy":
      return this.happyUrl;
      break;
    case "sad":
      return this.sadUrl;
      break;
    case "broken":
      return this.brokenUrl;
      break;
    case "idea":
      return this.ideaUrl;
      break;
    default:
      return null;
      break;
    }
  },

  isInputUrl: function FeedbackManager_isInputUrl(url) {
    /* Return true if the URL belongs to one of the pages of the Input website.  We can
     * identify these by looking for a domain of input.mozilla.com and a page name ending
     * in 'feedback', 'happy', or 'sad'.
     */
    let ioService = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService);
    try {
      let uri = ioService.newURI(url, null, null);
      let path = uri.path;
      if (uri.host == "input.mozilla.com") {
        if (path.indexOf("feedback") > -1 || path.indexOf("happy") > -1 || path.indexOf("sad") > -1) {
          return true;
        }
      }
    } catch(e) {
      /* newURI throws an exception if we try to parse urls like "about:config".  (Bug 644582)
       * Those are not the urls we're looking for anyway. */
      return false;
    }
    return false;
  },

  setCurrUrl: function FeedbackManager_setCurrUrl(url) {
    this._lastVisitedUrl = url;
  },

  fillInFeedbackPage: function FeedbackManager_fifp(url, window) {
    /* If the user activated the happy or sad feedback feature, a page
     * gets loaded containing input fields - either a single one containing id = "id_url",
     * or (potentially multiple) with class = "url".  Prefill all matching fields in with
     * the referring URL.
     */
    if (this.isInputUrl(url)) {
      if (this._lastVisitedUrl) {
        let tabbrowser = window.getBrowser();
        let currentBrowser = tabbrowser.selectedBrowser;
        let document = currentBrowser.contentDocument;
        let fields = document.getElementsByClassName("url");
        for (let i = 0; i < fields.length; i++) {
          fields[i].value = this._lastVisitedUrl;
        }
        let field = document.getElementById("id_url");
        if (field) {
           field.value = this._lastVisitedUrl;
        }
      }
    }
  }
};