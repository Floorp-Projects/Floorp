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
 *  Panos Astithas <past@mozilla.com>
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
// The maximum allowed number of concurrent timers per page.
const MAX_PAGE_TIMERS = 10000;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm");

function ConsoleAPI() {}
ConsoleAPI.prototype = {

  classID: Components.ID("{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),

  // nsIDOMGlobalPropertyInitializer
  init: function CA_init(aWindow) {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "inner-window-destroyed", false);

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
      time: function CA_time() {
        self.notifyObservers(outerID, innerID, "time", self.startTimer(innerID, arguments[0]));
      },
      timeEnd: function CA_timeEnd() {
        self.notifyObservers(outerID, innerID, "timeEnd", self.stopTimer(innerID, arguments[0]));
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
        groupEnd: "r",
        time: "r",
        timeEnd: "r"
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
      time: genPropDesc('time'),
      timeEnd: genPropDesc('timeEnd'),
      __noSuchMethod__: { enumerable: true, configurable: true, writable: true,
                          value: function() {} },
      __mozillaConsole__: { value: true }
    };

    Object.defineProperties(contentObj, properties);
    Cu.makeObjectPropsNormal(contentObj);

    return contentObj;
  },

  observe: function CA_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "xpcom-shutdown") {
      Services.obs.removeObserver(this, "xpcom-shutdown");
      Services.obs.removeObserver(this, "inner-window-destroyed");
    }
    else if (aTopic == "inner-window-destroyed") {
      let innerWindowID = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      delete this.timerRegistry[innerWindowID + ""];
    }
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
    if (typeof format != "string") {
      return aArguments;
    }
    // Format specification regular expression.
    let pattern = /%(\d*).?(\d*)[a-zA-Z]/g;
    let processed = format.replace(pattern, function CA_PA_substitute(spec) {
      switch (spec[spec.length-1]) {
        case "o":
        case "s":
          return String(args.shift());
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
  },

  /*
   * A registry of started timers. It contains a map of pages (defined by their
   * inner window IDs) to timer maps. Timer maps are key-value pairs of timer
   * names to timer start times, for all timers defined in that page. Timer
   * names are prepended with the inner window ID in order to avoid conflicts
   * with Object.prototype functions.
   */
  timerRegistry: {},

  /**
   * Create a new timer by recording the current time under the specified name.
   *
   * @param number aWindowId
   *        The inner ID of the window.
   * @param string aName
   *        The name of the timer.
   * @return object
   *        The name property holds the timer name and the started property
   *        holds the time the timer was started. In case of error, it returns
   *        an object with the single property "error" that contains the key
   *        for retrieving the localized error message.
   **/
  startTimer: function CA_startTimer(aWindowId, aName) {
    if (!aName) {
        return;
    }
    let innerID = aWindowId + "";
    if (!this.timerRegistry[innerID]) {
        this.timerRegistry[innerID] = {};
    }
    let pageTimers = this.timerRegistry[innerID];
    if (Object.keys(pageTimers).length > MAX_PAGE_TIMERS - 1) {
        return { error: "maxTimersExceeded" };
    }
    let key = aWindowId + "-" + aName.toString();
    if (!pageTimers[key]) {
        pageTimers[key] = Date.now();
    }
    return { name: aName, started: pageTimers[key] };
  },

  /**
   * Stop the timer with the specified name and retrieve the elapsed time.
   *
   * @param number aWindowId
   *        The inner ID of the window.
   * @param string aName
   *        The name of the timer.
   * @return object
   *        The name property holds the timer name and the duration property
   *        holds the number of milliseconds since the timer was started.
   **/
  stopTimer: function CA_stopTimer(aWindowId, aName) {
    if (!aName) {
        return;
    }
    let innerID = aWindowId + "";
    let pageTimers = this.timerRegistry[innerID];
    if (!pageTimers) {
        return;
    }
    let key = aWindowId + "-" + aName.toString();
    if (!pageTimers[key]) {
        return;
    }
    let duration = Date.now() - pageTimers[key];
    delete pageTimers[key];
    return { name: aName, duration: duration };
  }
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([ConsoleAPI]);
