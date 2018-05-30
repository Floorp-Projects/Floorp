/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const Services = require("Services");

// Match the function name from the result of toString() or toSource().
//
// Examples:
// (function foobar(a, b) { ...
// function foobar2(a) { ...
// function() { ...
const REGEX_MATCH_FUNCTION_NAME = /^\(?function\s+([^(\s]+)\s*\(/;

// Number of terminal entries for the self-xss prevention to go away
const CONSOLE_ENTRY_THRESHOLD = 5;

exports.CONSOLE_WORKER_IDS = [
  "SharedWorker",
  "ServiceWorker",
  "Worker"
];

var WebConsoleUtils = {

  CONSOLE_ENTRY_THRESHOLD,

  /**
   * Wrap a string in an nsISupportsString object.
   *
   * @param string string
   * @return nsISupportsString
   */
  supportsString: function(string) {
    let str = Cc["@mozilla.org/supports-string;1"]
              .createInstance(Ci.nsISupportsString);
    str.data = string;
    return str;
  },

  /**
   * Clone an object.
   *
   * @param object object
   *        The object you want cloned.
   * @param boolean recursive
   *        Tells if you want to dig deeper into the object, to clone
   *        recursively.
   * @param function [filter]
   *        Optional, filter function, called for every property. Three
   *        arguments are passed: key, value and object. Return true if the
   *        property should be added to the cloned object. Return false to skip
   *        the property.
   * @return object
   *         The cloned object.
   */
  cloneObject: function(object, recursive, filter) {
    if (typeof object != "object") {
      return object;
    }

    let temp;

    if (Array.isArray(object)) {
      temp = [];
      Array.forEach(object, function(value, index) {
        if (!filter || filter(index, value, object)) {
          temp.push(recursive ? WebConsoleUtils.cloneObject(value) : value);
        }
      });
    } else {
      temp = {};
      for (let key in object) {
        let value = object[key];
        if (object.hasOwnProperty(key) &&
            (!filter || filter(key, value, object))) {
          temp[key] = recursive ? WebConsoleUtils.cloneObject(value) : value;
        }
      }
    }

    return temp;
  },

  /**
   * Copies certain style attributes from one element to another.
   *
   * @param Node from
   *        The target node.
   * @param Node to
   *        The destination node.
   */
  copyTextStyles: function(from, to) {
    let win = from.ownerDocument.defaultView;
    let style = win.getComputedStyle(from);
    to.style.fontFamily = style.fontFamily;
    to.style.fontSize = style.fontSize;
    to.style.fontWeight = style.fontWeight;
    to.style.fontStyle = style.fontStyle;
  },

  /**
   * Determine if the given request mixes HTTP with HTTPS content.
   *
   * @param string request
   *        Location of the requested content.
   * @param string location
   *        Location of the current page.
   * @return boolean
   *         True if the content is mixed, false if not.
   */
  isMixedHTTPSRequest: function(request, location) {
    try {
      let requestURI = Services.io.newURI(request);
      let contentURI = Services.io.newURI(location);
      return (contentURI.scheme == "https" && requestURI.scheme != "https");
    } catch (ex) {
      return false;
    }
  },

  /**
   * Helper function to deduce the name of the provided function.
   *
   * @param function function
   *        The function whose name will be returned.
   * @return string
   *         Function name.
   */
  getFunctionName: function(func) {
    let name = null;
    if (func.name) {
      name = func.name;
    } else {
      let desc;
      try {
        desc = func.getOwnPropertyDescriptor("displayName");
      } catch (ex) {
        // Ignore.
      }
      if (desc && typeof desc.value == "string") {
        name = desc.value;
      }
    }
    if (!name) {
      try {
        let str = (func.toString() || func.toSource()) + "";
        name = (str.match(REGEX_MATCH_FUNCTION_NAME) || [])[1];
      } catch (ex) {
        // Ignore.
      }
    }
    return name;
  },

  /**
   * Get the object class name. For example, the |window| object has the Window
   * class name (based on [object Window]).
   *
   * @param object object
   *        The object you want to get the class name for.
   * @return string
   *         The object class name.
   */
  getObjectClassName: function(object) {
    if (object === null) {
      return "null";
    }
    if (object === undefined) {
      return "undefined";
    }

    let type = typeof object;
    if (type != "object") {
      // Grip class names should start with an uppercase letter.
      return type.charAt(0).toUpperCase() + type.substr(1);
    }

    let className;

    try {
      className = ((object + "").match(/^\[object (\S+)\]$/) || [])[1];
      if (!className) {
        className = ((object.constructor + "")
                     .match(/^\[object (\S+)\]$/) || [])[1];
      }
      if (!className && typeof object.constructor == "function") {
        className = this.getFunctionName(object.constructor);
      }
    } catch (ex) {
      // Ignore.
    }

    return className;
  },

  /**
   * Check if the given value is a grip with an actor.
   *
   * @param mixed grip
   *        Value you want to check if it is a grip with an actor.
   * @return boolean
   *         True if the given value is a grip with an actor.
   */
  isActorGrip: function(grip) {
    return grip && typeof (grip) == "object" && grip.actor;
  },

  /**
   * Value of devtools.selfxss.count preference
   *
   * @type number
   * @private
   */
  _usageCount: 0,
  get usageCount() {
    if (WebConsoleUtils._usageCount < CONSOLE_ENTRY_THRESHOLD) {
      WebConsoleUtils._usageCount =
        Services.prefs.getIntPref("devtools.selfxss.count");
      if (Services.prefs.getBoolPref("devtools.chrome.enabled")) {
        WebConsoleUtils.usageCount = CONSOLE_ENTRY_THRESHOLD;
      }
    }
    return WebConsoleUtils._usageCount;
  },
  set usageCount(newUC) {
    if (newUC <= CONSOLE_ENTRY_THRESHOLD) {
      WebConsoleUtils._usageCount = newUC;
      Services.prefs.setIntPref("devtools.selfxss.count", newUC);
    }
  },
  /**
   * The inputNode "paste" event handler generator. Helps prevent
   * self-xss attacks
   *
   * @param Element inputField
   * @param Element notificationBox
   * @returns A function to be added as a handler to 'paste' and
   *'drop' events on the input field
   */
  pasteHandlerGen: function(inputField, notificationBox, msg, okstring) {
    let handler = function(event) {
      if (WebConsoleUtils.usageCount >= CONSOLE_ENTRY_THRESHOLD) {
        inputField.removeEventListener("paste", handler);
        inputField.removeEventListener("drop", handler);
        return true;
      }
      if (notificationBox.getNotificationWithValue("selfxss-notification")) {
        event.preventDefault();
        event.stopPropagation();
        return false;
      }

      let notification = notificationBox.appendNotification(msg,
        "selfxss-notification", null,
        notificationBox.PRIORITY_WARNING_HIGH, null,
        function(eventType) {
          // Cleanup function if notification is dismissed
          if (eventType == "removed") {
            inputField.removeEventListener("keyup", pasteKeyUpHandler);
          }
        });

      function pasteKeyUpHandler(event2) {
        let value = inputField.value || inputField.textContent;
        if (value.includes(okstring)) {
          notificationBox.removeNotification(notification);
          inputField.removeEventListener("keyup", pasteKeyUpHandler);
          WebConsoleUtils.usageCount = CONSOLE_ENTRY_THRESHOLD;
        }
      }
      inputField.addEventListener("keyup", pasteKeyUpHandler);

      event.preventDefault();
      event.stopPropagation();
      return false;
    };
    return handler;
  },
};

exports.Utils = WebConsoleUtils;

