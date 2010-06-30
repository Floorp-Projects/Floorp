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

  setCurrUrl: function FeedbackManager_setCurrUrl(url) {
    this._lastVisitedUrl = url;
  },

  fillInFeedbackPage: function FeedbackManager_fifp(url, window) {
    /* If the user activated the happy or sad feedback feature, a page
     * gets loaded containing an input field id = id_url
     * Fill this field in with the referring URL.
     */
    if (url == this.happyUrl || url == this.sadUrl) {
      let tabbrowser = window.getBrowser();
      let currentBrowser = tabbrowser.selectedBrowser;
      let document = currentBrowser.contentDocument;
      let field = document.getElementById("id_url");
      if (field && this._lastVisitedUrl) {
        field.value = this._lastVisitedUrl;
      }
    }
  }
};