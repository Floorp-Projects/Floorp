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
    let uri = ioService.newURI(url, null, null);
    let path = uri.path;
    if (uri.host == "input.mozilla.com") {
      if (path.indexOf("feedback" > -1) || path.indexOf("happy" > -1) || path.indexOf("sad" > -1)) {
        return true;
      }
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