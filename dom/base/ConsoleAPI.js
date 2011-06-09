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
 *  Ryan Flint <rflint@mozilla.com>
 *  Rob Campbell <rcampbell@mozilla.com>
 *  Mihai Sucan <mihai.sucan@gmail.com>
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

  // nsIDOMGlobalPropertyInitializer
  init: function CA_init(aWindow) {
    let id;
    try {
      id = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils)
                  .outerWindowID;
    } catch (ex) {
      Cu.reportError(ex);
    }

    let self = this;
    let chromeObject = {
      // window.console API
      log: function CA_log() {
        self.notifyObservers(id, "log", arguments);
      },
      info: function CA_info() {
        self.notifyObservers(id, "info", arguments);
      },
      warn: function CA_warn() {
        self.notifyObservers(id, "warn", arguments);
      },
      error: function CA_error() {
        self.notifyObservers(id, "error", arguments);
      },
      debug: function CA_debug() {
        self.notifyObservers(id, "log", arguments);
      },
      trace: function CA_trace() {
        self.notifyObservers(id, "trace", self.getStackTrace());
      },
      // Displays an interactive listing of all the properties of an object.
      dir: function CA_dir() {
        self.notifyObservers(id, "dir", arguments);
      },
      __exposedProps__: {
        log: "r",
        info: "r",
        warn: "r",
        error: "r",
        debug: "r",
        trace: "r",
        dir: "r"
      }
    };

    // We need to return an actual content object here, instead of a wrapped
    // chrome object. This allows things like console.log.bind() to work.
    let contentObj = Cu.createObjectIn(aWindow);
    function genPropDesc(fun) {
      return { enumerable: true, configurable: true, writable: true,
               value: chromeObject[fun].bind(chromeObject) };
    }
    const properties = {
      log: genPropDesc('log'),
      info: genPropDesc('info'),
      warn: genPropDesc('warn'),
      error: genPropDesc('error'),
      debug: genPropDesc('debug'),
      trace: genPropDesc('trace'),
      dir: genPropDesc('dir'),
      __noSuchMethod__: { enumerable: true, configurable: true, writable: true,
                          value: function() {} },
      __mozillaConsole__: { value: true }
    };

    Object.defineProperties(contentObj, properties);
    Cu.makeObjectPropsNormal(contentObj);

    return contentObj;
  },

  /**
   * Notify all observers of any console API call
   **/
  notifyObservers: function CA_notifyObservers(aID, aLevel, aArguments) {
    if (!aID)
      return;

    let stack = this.getStackTrace();
    // Skip the first frame since it contains an internal call.
    let frame = stack[1];
    let consoleEvent = {
      ID: aID,
      level: aLevel,
      filename: frame.filename,
      lineNumber: frame.lineNumber,
      functionName: frame.functionName,
      arguments: aArguments
    };

    consoleEvent.wrappedJSObject = consoleEvent;

    Services.obs.notifyObservers(consoleEvent,
                                 "console-api-log-event", aID);
  },

  /**
   * Build the stacktrace array for the console.trace() call.
   *
   * @return array
   *         Each element is a stack frame that holds the following properties:
   *         filename, lineNumber, functionName and language.
   **/
  getStackTrace: function CA_getStackTrace() {
    let stack = [];
    let frame = Components.stack.caller;
    while (frame = frame.caller) {
      if (frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT ||
          frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT2) {
        stack.push({
          filename: frame.filename,
          lineNumber: frame.lineNumber,
          functionName: frame.name,
          language: frame.language,
        });
      }
    }

    return stack;
  }
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([ConsoleAPI]);
