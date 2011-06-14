/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm");

function ConsoleAPI() {}
ConsoleAPI.prototype = {

  classID: Components.ID("{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),

  // nsIDOMGlobalPropertyInitializer
  init: function CA_init(aWindow) {
    let outerID;
    let innerID;
    try {
      let windowUtils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);

      outerID = windowUtils.outerWindowID;
      innerID = windowUtils.currentInnerWindowID;
    }
    catch (ex) {
      Cu.reportError(ex);
    }

    let self = this;
    let chromeObject = {
      // window.console API
      log: function CA_log() {
        self.notifyObservers(outerID, innerID, "log", self.processArguments(arguments));
      },
      info: function CA_info() {
        self.notifyObservers(outerID, innerID, "info", self.processArguments(arguments));
      },
      warn: function CA_warn() {
        self.notifyObservers(outerID, innerID, "warn", self.processArguments(arguments));
      },
      error: function CA_error() {
        self.notifyObservers(outerID, innerID, "error", self.processArguments(arguments));
      },
      debug: function CA_debug() {
        self.notifyObservers(outerID, innerID, "log", self.processArguments(arguments));
      },
      trace: function CA_trace() {
        self.notifyObservers(outerID, innerID, "trace", self.getStackTrace());
      },
      // Displays an interactive listing of all the properties of an object.
      dir: function CA_dir() {
        self.notifyObservers(outerID, innerID, "dir", arguments);
      },
      group: function CA_group() {
        self.notifyObservers(outerID, innerID, "group", self.beginGroup(arguments));
      },
      groupCollapsed: function CA_groupCollapsed() {
        self.notifyObservers(outerID, innerID, "groupCollapsed", self.beginGroup(arguments));
      },
      groupEnd: function CA_groupEnd() {
        self.notifyObservers(outerID, innerID, "groupEnd", arguments);
      },
      __exposedProps__: {
        log: "r",
        info: "r",
        warn: "r",
        error: "r",
        debug: "r",
        trace: "r",
        dir: "r",
        group: "r",
        groupCollapsed: "r",
        groupEnd: "r"
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
      group: genPropDesc('group'),
      groupCollapsed: genPropDesc('groupCollapsed'),
      groupEnd: genPropDesc('groupEnd'),
      __noSuchMethod__: { enumerable: true, configurable: true, writable: true,
                          value: function() {} },
      __mozillaConsole__: { value: true }
    };

    Object.defineProperties(contentObj, properties);
    Cu.makeObjectPropsNormal(contentObj);

    return contentObj;
  },

  /**
   * Notify all observers of any console API call.
   *
   * @param number aOuterWindowID
   *        The outer window ID from where the message came from.
   * @param number aInnerWindowID
   *        The inner window ID from where the message came from.
   * @param string aLevel
   *        The message level.
   * @param mixed aArguments
   *        The arguments given to the console API call.
   **/
  notifyObservers:
  function CA_notifyObservers(aOuterWindowID, aInnerWindowID, aLevel, aArguments) {
    if (!aOuterWindowID) {
      return;
    }

    let stack = this.getStackTrace();
    // Skip the first frame since it contains an internal call.
    let frame = stack[1];
    let consoleEvent = {
      ID: aOuterWindowID,
      innerID: aInnerWindowID,
      level: aLevel,
      filename: frame.filename,
      lineNumber: frame.lineNumber,
      functionName: frame.functionName,
      arguments: aArguments
    };

    consoleEvent.wrappedJSObject = consoleEvent;

    ConsoleAPIStorage.recordEvent(aInnerWindowID, consoleEvent);

    Services.obs.notifyObservers(consoleEvent,
                                 "console-api-log-event", aOuterWindowID);
  },

  /**
   * Process the console API call arguments in order to perform printf-like
   * string substitution.
   * TODO: object substitution should display an interactive property list (bug
   * 685815) and width and precision qualifiers should be taken into account
   * (bug 685813).
   *
   * @param mixed aArguments
   *        The arguments given to the console API call.
   **/
  processArguments: function CA_processArguments(aArguments) {
    if (aArguments.length < 2) {
      return aArguments;
    }
    let args = Array.prototype.slice.call(aArguments);
    let format = args.shift();
    // Format specification regular expression.
    let pattern = /%(\d*).?(\d*)[a-zA-Z]/g;
    let processed = format.replace(pattern, function CA_PA_substitute(spec) {
      switch (spec[spec.length-1]) {
        case "o":
        case "s":
          return args.shift().toString();
        case "d":
        case "i":
          return parseInt(args.shift());
        case "f":
          return parseFloat(args.shift());
        default:
          return spec;
      };
    });
    args.unshift(processed);
    return args;
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
  },

  /**
   * Begin a new group for logging output together.
   **/
  beginGroup: function CA_beginGroup() {
    return Array.prototype.join.call(arguments[0], " ");
  }
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([ConsoleAPI]);
