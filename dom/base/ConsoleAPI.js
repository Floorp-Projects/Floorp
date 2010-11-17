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
 * The Original Code is Console API code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Dahl <ddahl@mozilla.com>  (Original Author)
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

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function ConsoleAPI() {}
ConsoleAPI.prototype = {

  classID: Components.ID("{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),

  // The ID of our attached window
  id: null,

  // nsIDOMGlobalPropertyInitializer
  // Associates us with the window we're attached to.
  init: function CA_init(aWindow) {
    try {
      this.id = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils)
                       .outerWindowID;
    } catch (ex) {
      Cu.reportError(ex);
    }

    return this;
  },

  // window.console API
  log: function CA_log() {
    this.notifyObservers("log", arguments);
  },
  info: function CA_info() {
    this.notifyObservers("info", arguments);
  },
  warn: function CA_warn() {
    this.notifyObservers("warn", arguments);
  },
  error: function CA_error() {
    this.notifyObservers("error", arguments);
  },

  __exposedProps__: {
    log: "r",
    info: "r",
    warn: "r",
    error: "r"
  },

  /**
   * Notify all observers of any console API call
   **/
  notifyObservers: function CA_notifyObservers(aLevel, aArguments) {
    let consoleEvent = {
      ID: this.id,
      level: aLevel,
      arguments: aArguments
    };

    consoleEvent.wrappedJSObject = consoleEvent;

    Services.obs.notifyObservers(consoleEvent,
                                 "console-api-log-event", this.id);
  }
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([ConsoleAPI]);
