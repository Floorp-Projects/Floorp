/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Define a 'console' API to roughly match the implementation provided by
 * Firebug.
 * This module helps cases where code is shared between the web and Firefox.
 * See also Browser.jsm for an implementation of other web constants to help
 * sharing code between the web and firefox;
 *
 * The API is only be a rough approximation for 3 reasons:
 * - The Firebug console API is implemented in many places with differences in
 *   the implementations, so there isn't a single reference to adhere to
 * - The Firebug console is a rich display compared with dump(), so there will
 *   be many things that we can't replicate
 * - The primary use of this API is debugging and error logging so the perfect
 *   implementation isn't always required (or even well defined)
 */

const EXPORTED_SYMBOLS = [ "console" ];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * String utility to ensure that strings are a specified length. Strings
 * that are too long are truncated to the max length and the last char is
 * set to "_". Strings that are too short are left padded with spaces.
 *
 * @param {string} aStr
 *        The string to format to the correct length
 * @param {number} aMaxLen
 *        The maximum allowed length of the returned string
 * @param {number} aMinLen (optional)
 *        The minimum allowed length of the returned string. If undefined,
 *        then aMaxLen will be used
 * @param {object} aOptions (optional)
 *        An object allowing format customization. The only customization
 *        allowed currently is 'truncate' which can take the value "start" to
 *        truncate strings from the start as opposed to the end.
 * @return {string}
 *        The original string formatted to fit the specified lengths
 */
function fmt(aStr, aMaxLen, aMinLen, aOptions) {
  if (aMinLen == null) {
    aMinLen = aMaxLen;
  }
  if (aStr == null) {
    aStr = "";
  }
  if (aStr.length > aMaxLen) {
    if (aOptions && aOptions.truncate == "start") {
      return "_" + aStr.substring(aStr.length - aMaxLen + 1);
    }
    else {
      return aStr.substring(0, aMaxLen - 1) + "_";
    }
  }
  if (aStr.length < aMinLen) {
    return Array(aMinLen - aStr.length + 1).join(" ") + aStr;
  }
  return aStr;
}

/**
 * Utility to extract the constructor name of an object.
 * Object.toString gives: "[object ?????]"; we want the "?????".
 *
 * @param {object} aObj
 *        The object from which to extract the constructor name
 * @return {string}
 *        The constructor name
 */
function getCtorName(aObj) {
  if (aObj.constructor && aObj.constructor.name) {
    return aObj.constructor.name;
  }
  // If that fails, use Objects toString which sometimes gives something
  // better than 'Object', and at least defaults to Object if nothing better
  return Object.prototype.toString.call(aObj).slice(8, -1);
}

/**
 * A single line stringification of an object designed for use by humans
 *
 * @param {any} aThing
 *        The object to be stringified
 * @return {string}
 *        A single line representation of aThing, which will generally be at
 *        most 80 chars long
 */
function stringify(aThing) {
  if (aThing === undefined) {
    return "undefined";
  }

  if (aThing === null) {
    return "null";
  }

  if (typeof aThing == "object") {
    let type = getCtorName(aThing);
    if (aThing instanceof Components.interfaces.nsIDOMNode && aThing.tagName) {
      return debugElement(aThing);
    }
    type = (type == "Object" ? "" : type + " ");
    let json;
    try {
      json = JSON.stringify(aThing);
    }
    catch (ex) {
      // Can't use a real ellipsis here, because cmd.exe isn't unicode-enabled
      json = "{" + Object.keys(aThing).join(":..,") + ":.., " + "}";
    }
    return type + fmt(json, 50, 0);
  }

  if (typeof aThing == "function") {
    return fmt(aThing.toString().replace(/\s+/g, " "), 80, 0);
  }

  let str = aThing.toString().replace(/\n/g, "|");
  return fmt(str, 80, 0);
}

/**
 * Create a simple debug representation of a given element.
 *
 * @param {nsIDOMElement} aElement
 *        The element to debug
 * @return {string}
 *        A simple single line representation of aElement
 */
function debugElement(aElement) {
  return "<" + aElement.tagName +
      (aElement.id ? "#" + aElement.id : "") +
      (aElement.className ?
          "." + aElement.className.split(" ").join(" .") :
          "") +
      ">";
}

/**
 * A multi line stringification of an object, designed for use by humans
 *
 * @param {any} aThing
 *        The object to be stringified
 * @return {string}
 *        A multi line representation of aThing
 */
function log(aThing) {
  if (aThing === null) {
    return "null\n";
  }

  if (aThing === undefined) {
    return "undefined\n";
  }

  if (typeof aThing == "object") {
    let reply = "";
    let type = getCtorName(aThing);
    if (type == "Error") {
      reply += "  " + aThing.message + "\n";
      reply += logProperty("stack", aThing.stack);
    }
    else if (aThing instanceof Components.interfaces.nsIDOMNode && aThing.tagName) {
      reply += "  " + debugElement(aThing) + "\n";
    }
    else {
      let keys = Object.getOwnPropertyNames(aThing);
      if (keys.length > 0) {
        reply += type + "\n";
        keys.forEach(function(aProp) {
          reply += logProperty(aProp, aThing[aProp]);
        }, this);
      }
      else {
        reply += type + "\n";
        let root = aThing;
        let logged = [];
        while (root != null) {
          let properties = Object.keys(root);
          properties.sort();
          properties.forEach(function(property) {
            if (!(property in logged)) {
              logged[property] = property;
              reply += logProperty(property, aThing[property]);
            }
          });

          root = Object.getPrototypeOf(root);
          if (root != null) {
            reply += '  - prototype ' + getCtorName(root) + '\n';
          }
        }
      }
    }

    return reply;
  }

  return "  " + aThing.toString() + "\n";
}

/**
 * Helper for log() which converts a property/value pair into an output
 * string
 *
 * @param {string} aProp
 *        The name of the property to include in the output string
 * @param {object} aValue
 *        Value assigned to aProp to be converted to a single line string
 * @return {string}
 *        Multi line output string describing the property/value pair
 */
function logProperty(aProp, aValue) {
  let reply = "";
  if (aProp == "stack" && typeof value == "string") {
    let trace = parseStack(aValue);
    reply += formatTrace(trace);
  }
  else {
    reply += "    - " + aProp + " = " + stringify(aValue) + "\n";
  }
  return reply;
}

/**
 * Parse a stack trace, returning an array of stack frame objects, where
 * each has file/line/call members
 *
 * @param {string} aStack
 *        The serialized stack trace
 * @return {object[]}
 *        Array of { file: "...", line: NNN, call: "..." } objects
 */
function parseStack(aStack) {
  let trace = [];
  aStack.split("\n").forEach(function(line) {
    if (!line) {
      return;
    }
    let at = line.lastIndexOf("@");
    let posn = line.substring(at + 1);
    trace.push({
      file: posn.split(":")[0],
      line: posn.split(":")[1],
      call: line.substring(0, at)
    });
  }, this);
  return trace;
}

/**
 * parseStack() takes output from an exception from which it creates the an
 * array of stack frame objects, this has the same output but using data from
 * Components.stack
 *
 * @param {string} aFrame
 *        The stack frame from which to begin the walk
 * @return {object[]}
 *        Array of { file: "...", line: NNN, call: "..." } objects
 */
function getStack(aFrame) {
  if (!aFrame) {
    aFrame = Components.stack.caller;
  }
  let trace = [];
  while (aFrame) {
    trace.push({
      file: aFrame.filename,
      line: aFrame.lineNumber,
      call: aFrame.name
    });
    aFrame = aFrame.caller;
  }
  return trace;
}

/**
 * Take the output from parseStack() and convert it to nice readable
 * output
 *
 * @param {object[]} aTrace
 *        Array of trace objects as created by parseStack()
 * @return {string} Multi line report of the stack trace
 */
function formatTrace(aTrace) {
  let reply = "";
  aTrace.forEach(function(frame) {
    reply += fmt(frame.file, 20, 20, { truncate: "start" }) + " " +
             fmt(frame.line, 5, 5) + " " +
             fmt(frame.call, 75, 75) + "\n";
  });
  return reply;
}

/**
 * Create a function which will output a concise level of output when used
 * as a logging function
 *
 * @param {string} aLevel
 *        A prefix to all output generated from this function detailing the
 *        level at which output occurred
 * @return {function}
 *        A logging function
 * @see createMultiLineDumper()
 */
function createDumper(aLevel) {
  return function() {
    let args = Array.prototype.slice.call(arguments, 0);
    let data = args.map(function(arg) {
      return stringify(arg);
    });
    dump(aLevel + ": " + data.join(", ") + "\n");
  };
}

/**
 * Create a function which will output more detailed level of output when
 * used as a logging function
 *
 * @param {string} aLevel
 *        A prefix to all output generated from this function detailing the
 *        level at which output occurred
 * @return {function}
 *        A logging function
 * @see createDumper()
 */
function createMultiLineDumper(aLevel) {
  return function() {
    dump(aLevel + "\n");
    let args = Array.prototype.slice.call(arguments, 0);
    args.forEach(function(arg) {
      dump(log(arg));
    });
  };
}

/**
 * This creates a console object that somewhat replicates Firebug's console
 * object. It currently writes to dump(), but should write to the web
 * console's chrome error section (when it has one)
 */
const console = {
  debug: createMultiLineDumper("debug"),
  log: createDumper("log"),
  info: createDumper("info"),
  warn: createDumper("warn"),
  error: createMultiLineDumper("error"),

  trace: function Console_trace() {
    let trace = getStack(Components.stack.caller);
    dump(formatTrace(trace) + "\n");
  },
  clear: function Console_clear() {},

  dir: createMultiLineDumper("dir"),
  dirxml: createMultiLineDumper("dirxml"),
  group: createDumper("group"),
  groupEnd: createDumper("groupEnd")
};
