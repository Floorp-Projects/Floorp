/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["WebConsoleUtils", "JSPropertyProvider"];

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";

const TYPES = { OBJECT: 0,
                FUNCTION: 1,
                ARRAY: 2,
                OTHER: 3,
                ITERATOR: 4,
                GETTER: 5,
                GENERATOR: 6,
                STRING: 7
              };

var gObjectId = 0;

var WebConsoleUtils = {
  TYPES: TYPES,

  /**
   * Convenience function to unwrap a wrapped object.
   *
   * @param aObject the object to unwrap.
   * @return aObject unwrapped.
   */
  unwrap: function WCU_unwrap(aObject)
  {
    try {
      return XPCNativeWrapper.unwrap(aObject);
    }
    catch (ex) {
      return aObject;
    }
  },

  /**
   * Wrap a string in an nsISupportsString object.
   *
   * @param string aString
   * @return nsISupportsString
   */
  supportsString: function WCU_supportsString(aString)
  {
    let str = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
    str.data = aString;
    return str;
  },

  /**
   * Clone an object.
   *
   * @param object aObject
   *        The object you want cloned.
   * @param boolean aRecursive
   *        Tells if you want to dig deeper into the object, to clone
   *        recursively.
   * @param function [aFilter]
   *        Optional, filter function, called for every property. Three
   *        arguments are passed: key, value and object. Return true if the
   *        property should be added to the cloned object. Return false to skip
   *        the property.
   * @return object
   *         The cloned object.
   */
  cloneObject: function WCU_cloneObject(aObject, aRecursive, aFilter)
  {
    if (typeof aObject != "object") {
      return aObject;
    }

    let temp;

    if (Array.isArray(aObject)) {
      temp = [];
      Array.forEach(aObject, function(aValue, aIndex) {
        if (!aFilter || aFilter(aIndex, aValue, aObject)) {
          temp.push(aRecursive ? WCU_cloneObject(aValue) : aValue);
        }
      });
    }
    else {
      temp = {};
      for (let key in aObject) {
        let value = aObject[key];
        if (aObject.hasOwnProperty(key) &&
            (!aFilter || aFilter(key, value, aObject))) {
          temp[key] = aRecursive ? WCU_cloneObject(value) : value;
        }
      }
    }

    return temp;
  },

  /**
   * Gets the ID of the inner window of this DOM window.
   *
   * @param nsIDOMWindow aWindow
   * @return integer
   *         Inner ID for the given aWindow.
   */
  getInnerWindowId: function WCU_getInnerWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
  },

  /**
   * Gets the ID of the outer window of this DOM window.
   *
   * @param nsIDOMWindow aWindow
   * @return integer
   *         Outer ID for the given aWindow.
   */
  getOuterWindowId: function WCU_getOuterWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  },

  /**
   * Gets the window that has the given outer ID.
   *
   * @param integer aOuterId
   * @param nsIDOMWindow [aHintWindow]
   *        Optional, the window object used to QueryInterface to
   *        nsIDOMWindowUtils. If this is not given,
   *        Services.wm.getMostRecentWindow() is used.
   * @return nsIDOMWindow|null
   *         The window object with the given outer ID.
   */
  getWindowByOuterId: function WCU_getWindowByOuterId(aOuterId, aHintWindow)
  {
    let someWindow = aHintWindow || Services.wm.getMostRecentWindow(null);
    let content = null;

    if (someWindow) {
      let windowUtils = someWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                                   getInterface(Ci.nsIDOMWindowUtils);
      content = windowUtils.getOuterWindowWithId(aOuterId);
    }

    return content;
  },

  /**
   * Gets the window that has the given inner ID.
   *
   * @param integer aInnerId
   * @param nsIDOMWindow [aHintWindow]
   *        Optional, the window object used to QueryInterface to
   *        nsIDOMWindowUtils. If this is not given,
   *        Services.wm.getMostRecentWindow() is used.
   * @return nsIDOMWindow|null
   *         The window object with the given inner ID.
   */
  getWindowByInnerId: function WCU_getWindowByInnerId(aInnerId, aHintWindow)
  {
    let someWindow = aHintWindow || Services.wm.getMostRecentWindow(null);
    let content = null;

    if (someWindow) {
      let windowUtils = someWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                                   getInterface(Ci.nsIDOMWindowUtils);
      content = windowUtils.getInnerWindowWithId(aInnerId);
    }

    return content;
  },

  /**
   * Abbreviates the given source URL so that it can be displayed flush-right
   * without being too distracting.
   *
   * @param string aSourceURL
   *        The source URL to shorten.
   * @return string
   *         The abbreviated form of the source URL.
   */
  abbreviateSourceURL: function WCU_abbreviateSourceURL(aSourceURL)
  {
    // Remove any query parameters.
    let hookIndex = aSourceURL.indexOf("?");
    if (hookIndex > -1) {
      aSourceURL = aSourceURL.substring(0, hookIndex);
    }

    // Remove a trailing "/".
    if (aSourceURL[aSourceURL.length - 1] == "/") {
      aSourceURL = aSourceURL.substring(0, aSourceURL.length - 1);
    }

    // Remove all but the last path component.
    let slashIndex = aSourceURL.lastIndexOf("/");
    if (slashIndex > -1) {
      aSourceURL = aSourceURL.substring(slashIndex + 1);
    }

    return aSourceURL;
  },

  /**
   * Format the jsterm execution result based on its type.
   *
   * @param mixed aResult
   *        The evaluation result object you want displayed.
   * @return string
   *         The string that can be displayed.
   */
  formatResult: function WCU_formatResult(aResult)
  {
    let output = "";
    let type = this.getResultType(aResult);

    switch (type) {
      case "string":
        output = this.formatResultString(aResult);
        break;
      case "boolean":
      case "date":
      case "error":
      case "number":
      case "regexp":
        output = aResult.toString();
        break;
      case "null":
      case "undefined":
        output = type;
        break;
      default:
        try {
          if (aResult.toSource) {
            output = aResult.toSource();
          }
          if (!output || output == "({})") {
            output = aResult + "";
          }
        }
        catch (ex) {
          output = ex;
        }
        break;
    }

    return output + "";
  },

  /**
   * Format a string for output.
   *
   * @param string aString
   *        The string you want to display.
   * @return string
   *         The string that can be displayed.
   */
  formatResultString: function WCU_formatResultString(aString)
  {
    function isControlCode(c) {
      // See http://en.wikipedia.org/wiki/C0_and_C1_control_codes
      // C0 is 0x00-0x1F, C1 is 0x80-0x9F (inclusive).
      // We also include DEL (U+007F) and NBSP (U+00A0), which are not strictly
      // in C1 but border it.
      return (c <= 0x1F) || (0x7F <= c && c <= 0xA0);
    }

    function replaceFn(aMatch, aType, aHex) {
      // Leave control codes escaped, but unescape the rest of the characters.
      let c = parseInt(aHex, 16);
      return isControlCode(c) ? aMatch : String.fromCharCode(c);
    }

    let output = uneval(aString).replace(/\\(x)([0-9a-fA-F]{2})/g, replaceFn)
                 .replace(/\\(u)([0-9a-fA-F]{4})/g, replaceFn);

    return output;
  },

  /**
   * Determine if an object can be inspected or not.
   *
   * @param mixed aObject
   *        The object you want to check if it can be inspected.
   * @return boolean
   *         True if the object is inspectable or false otherwise.
   */
  isObjectInspectable: function WCU_isObjectInspectable(aObject)
  {
    let isEnumerable = false;

    // Skip Iterators and Generators.
    if (this.isIteratorOrGenerator(aObject)) {
      return false;
    }

    try {
      for (let p in aObject) {
        isEnumerable = true;
        break;
      }
    }
    catch (ex) {
      // Proxy objects can lack an enumerable method.
    }

    return isEnumerable && typeof(aObject) != "string";
  },

  /**
   * Determine the type of the jsterm execution result.
   *
   * @param mixed aResult
   *        The evaluation result object you want to check.
   * @return string
   *         Constructor name or type: string, number, boolean, regexp, date,
   *         function, object, null, undefined...
   */
  getResultType: function WCU_getResultType(aResult)
  {
    let type = aResult === null ? "null" : typeof aResult;
    if (type == "object" && aResult.constructor && aResult.constructor.name) {
      type = aResult.constructor.name;
    }

    return type.toLowerCase();
  },

  /**
   * Figures out the type of aObject and the string to display as the object
   * value.
   *
   * @see TYPES
   * @param object aObject
   *        The object to operate on.
   * @return object
   *         An object of the form:
   *         {
   *           type: TYPES.OBJECT || TYPES.FUNCTION || ...
   *           display: string for displaying the object
   *         }
   */
  presentableValueFor: function WCU_presentableValueFor(aObject)
  {
    let type = this.getResultType(aObject);
    let presentable;

    switch (type) {
      case "undefined":
      case "null":
        return {
          type: TYPES.OTHER,
          display: type
        };

      case "array":
        return {
          type: TYPES.ARRAY,
          display: "Array"
        };

      case "string":
        return {
          type: TYPES.STRING,
          display: "\"" + aObject + "\""
        };

      case "date":
      case "regexp":
      case "number":
      case "boolean":
        return {
          type: TYPES.OTHER,
          display: aObject.toString()
        };

      case "iterator":
        return {
          type: TYPES.ITERATOR,
          display: "Iterator"
        };

      case "function":
        presentable = aObject.toString();
        return {
          type: TYPES.FUNCTION,
          display: presentable.substring(0, presentable.indexOf(')') + 1)
        };

      default:
        presentable = String(aObject);
        let m = /^\[object (\S+)\]/.exec(presentable);

        try {
          if (typeof aObject == "object" && typeof aObject.next == "function" &&
              m && m[1] == "Generator") {
            return {
              type: TYPES.GENERATOR,
              display: m[1]
            };
          }
        }
        catch (ex) {
          // window.history.next throws in the typeof check above.
          return {
            type: TYPES.OBJECT,
            display: m ? m[1] : "Object"
          };
        }

        if (typeof aObject == "object" &&
            typeof aObject.__iterator__ == "function") {
          return {
            type: TYPES.ITERATOR,
            display: "Iterator"
          };
        }

        return {
          type: TYPES.OBJECT,
          display: m ? m[1] : "Object"
        };
    }
  },

  /**
   * Tells if the given function is native or not.
   *
   * @param function aFunction
   *        The function you want to check if it is native or not.
   * @return boolean
   *         True if the given function is native, false otherwise.
   */
  isNativeFunction: function WCU_isNativeFunction(aFunction)
  {
    return typeof aFunction == "function" && !("prototype" in aFunction);
  },

  /**
   * Tells if the given property of the provided object is a non-native getter or
   * not.
   *
   * @param object aObject
   *        The object that contains the property.
   * @param string aProp
   *        The property you want to check if it is a getter or not.
   * @return boolean
   *         True if the given property is a getter, false otherwise.
   */
  isNonNativeGetter: function WCU_isNonNativeGetter(aObject, aProp)
  {
    if (typeof aObject != "object") {
      return false;
    }
    let desc;
    while (aObject) {
      try {
        if (desc = Object.getOwnPropertyDescriptor(aObject, aProp)) {
          break;
        }
      }
      catch (ex) {
        // Native getters throw here. See bug 520882.
        if (ex.name == "NS_ERROR_XPC_BAD_CONVERT_JS" ||
            ex.name == "NS_ERROR_XPC_BAD_OP_ON_WN_PROTO") {
          return false;
        }
        throw ex;
      }
      aObject = Object.getPrototypeOf(aObject);
    }
    if (desc && desc.get && !this.isNativeFunction(desc.get)) {
      return true;
    }
    return false;
  },

  /**
   * Get an array that describes the properties of the given object.
   *
   * @param object aObject
   *        The object to get the properties from.
   * @param object aObjectCache
   *        Optional object cache where to store references to properties of
   *        aObject that are inspectable. See this.isObjectInspectable().
   * @return array
   *         An array that describes each property from the given object. Each
   *         array element is an object (a property descriptor). Each property
   *         descriptor has the following properties:
   *         - name - property name
   *         - value - a presentable property value representation (see
   *                   this.presentableValueFor())
   *         - type - value type (see this.presentableValueFor())
   *         - inspectable - tells if the property value holds further
   *                         properties (see this.isObjectInspectable()).
   *         - objectId - optional, available only if aObjectCache is given and
   *         if |inspectable| is true. You can do
   *         aObjectCache[propertyDescriptor.objectId] to get the actual object
   *         referenced by the property of aObject.
   */
  namesAndValuesOf: function WCU_namesAndValuesOf(aObject, aObjectCache)
  {
    let pairs = [];
    let value, presentable;

    let isDOMDocument = aObject instanceof Ci.nsIDOMDocument;
    let deprecated = ["width", "height", "inputEncoding"];

    for (let propName in aObject) {
      // See bug 632275: skip deprecated properties.
      if (isDOMDocument && deprecated.indexOf(propName) > -1) {
        continue;
      }

      // Also skip non-native getters.
      if (this.isNonNativeGetter(aObject, propName)) {
        value = "";
        presentable = {type: TYPES.GETTER, display: "Getter"};
      }
      else {
        try {
          value = aObject[propName];
          presentable = this.presentableValueFor(value);
        }
	      catch (ex) {
          continue;
        }
      }

      let pair = {};
      pair.name = propName;
      pair.value = presentable.display;
      pair.inspectable = false;
      pair.type = presentable.type;

      switch (presentable.type) {
        case TYPES.GETTER:
        case TYPES.ITERATOR:
        case TYPES.GENERATOR:
        case TYPES.STRING:
          break;
        default:
          try {
            for (let p in value) {
              pair.inspectable = true;
              break;
            }
          }
          catch (ex) { }
          break;
      }

      // Store the inspectable object.
      if (pair.inspectable && aObjectCache) {
        pair.objectId = ++gObjectId;
        aObjectCache[pair.objectId] = value;
      }

      pairs.push(pair);
    }

    pairs.sort(function(a, b)
    {
      // Convert the pair.name to a number for later sorting.
      let aNumber = parseFloat(a.name);
      let bNumber = parseFloat(b.name);

      // Sort numbers.
      if (!isNaN(aNumber) && isNaN(bNumber)) {
        return -1;
      }
      else if (isNaN(aNumber) && !isNaN(bNumber)) {
        return 1;
      }
      else if (!isNaN(aNumber) && !isNaN(bNumber)) {
        return aNumber - bNumber;
      }
      // Sort string.
      else if (a.name < b.name) {
        return -1;
      }
      else if (a.name > b.name) {
        return 1;
      }
      else {
        return 0;
      }
    });

    return pairs;
  },

  /**
   * Check if the given object is an iterator or a generator.
   *
   * @param object aObject
   *        The object you want to check.
   * @return boolean
   *         True if the given object is an iterator or a generator, otherwise
   *         false is returned.
   */
  isIteratorOrGenerator: function WCU_isIteratorOrGenerator(aObject)
  {
    if (aObject === null) {
      return false;
    }

    if (typeof aObject == "object") {
      if (typeof aObject.__iterator__ == "function" ||
          aObject.constructor && aObject.constructor.name == "Iterator") {
        return true;
      }

      try {
        let str = aObject.toString();
        if (typeof aObject.next == "function" &&
            str.indexOf("[object Generator") == 0) {
          return true;
        }
      }
      catch (ex) {
        // window.history.next throws in the typeof check above.
        return false;
      }
    }

    return false;
  },
};

//////////////////////////////////////////////////////////////////////////
// Localization
//////////////////////////////////////////////////////////////////////////

WebConsoleUtils.l10n = {
  /**
   * Generates a formatted timestamp string for displaying in console messages.
   *
   * @param integer [aMilliseconds]
   *        Optional, allows you to specify the timestamp in milliseconds since
   *        the UNIX epoch.
   * @return string
   *         The timestamp formatted for display.
   */
  timestampString: function WCU_l10n_timestampString(aMilliseconds)
  {
    let d = new Date(aMilliseconds ? aMilliseconds : null);
    let hours = d.getHours(), minutes = d.getMinutes();
    let seconds = d.getSeconds(), milliseconds = d.getMilliseconds();
    let parameters = [hours, minutes, seconds, milliseconds];
    return this.getFormatStr("timestampFormat", parameters);
  },

  /**
   * Retrieve a localized string.
   *
   * @param string aName
   *        The string name you want from the Web Console string bundle.
   * @return string
   *         The localized string.
   */
  getStr: function WCU_l10n_getStr(aName)
  {
    let result;
    try {
      result = this.stringBundle.GetStringFromName(aName);
    }
    catch (ex) {
      Cu.reportError("Failed to get string: " + aName);
      throw ex;
    }
    return result;
  },

  /**
   * Retrieve a localized string formatted with values coming from the given
   * array.
   *
   * @param string aName
   *        The string name you want from the Web Console string bundle.
   * @param array aArray
   *        The array of values you want in the formatted string.
   * @return string
   *         The formatted local string.
   */
  getFormatStr: function WCU_l10n_getFormatStr(aName, aArray)
  {
    let result;
    try {
      result = this.stringBundle.formatStringFromName(aName, aArray, aArray.length);
    }
    catch (ex) {
      Cu.reportError("Failed to format string: " + aName);
      throw ex;
    }
    return result;
  },
};

XPCOMUtils.defineLazyGetter(WebConsoleUtils.l10n, "stringBundle", function() {
  return Services.strings.createBundle(STRINGS_URI);
});


//////////////////////////////////////////////////////////////////////////
// JS Completer
//////////////////////////////////////////////////////////////////////////

var JSPropertyProvider = (function _JSPP(WCU) {
const STATE_NORMAL = 0;
const STATE_QUOTE = 2;
const STATE_DQUOTE = 3;

const OPEN_BODY = "{[(".split("");
const CLOSE_BODY = "}])".split("");
const OPEN_CLOSE_BODY = {
  "{": "}",
  "[": "]",
  "(": ")",
};

/**
 * Analyses a given string to find the last statement that is interesting for
 * later completion.
 *
 * @param   string aStr
 *          A string to analyse.
 *
 * @returns object
 *          If there was an error in the string detected, then a object like
 *
 *            { err: "ErrorMesssage" }
 *
 *          is returned, otherwise a object like
 *
 *            {
 *              state: STATE_NORMAL|STATE_QUOTE|STATE_DQUOTE,
 *              startPos: index of where the last statement begins
 *            }
 */
function findCompletionBeginning(aStr)
{
  let bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;
  for (let i = 0; i < aStr.length; i++) {
    c = aStr[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        }
        else if (c == "'") {
          state = STATE_QUOTE;
        }
        else if (c == ";") {
          start = i + 1;
        }
        else if (c == " ") {
          start = i + 1;
        }
        else if (OPEN_BODY.indexOf(c) != -1) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        }
        else if (CLOSE_BODY.indexOf(c) != -1) {
          var last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error"
            };
          }
          if (c == "}") {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == "\\") {
          i++;
        }
        else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Single quote state > ' <
      case STATE_QUOTE:
        if (c == "\\") {
          i++;
        }
        else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == "'") {
          state = STATE_NORMAL;
        }
        break;
    }
  }

  return {
    state: state,
    startPos: start
  };
}

/**
 * Provides a list of properties, that are possible matches based on the passed
 * scope and inputValue.
 *
 * @param object aScope
 *        Scope to use for the completion.
 *
 * @param string aInputValue
 *        Value that should be completed.
 *
 * @returns null or object
 *          If no completion valued could be computed, null is returned,
 *          otherwise a object with the following form is returned:
 *            {
 *              matches: [ string, string, string ],
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *            }
 */
function JSPropertyProvider(aScope, aInputValue)
{
  let obj = WCU.unwrap(aScope);

  // Analyse the aInputValue and find the beginning of the last part that
  // should be completed.
  let beginning = findCompletionBeginning(aInputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  let completionPart = aInputValue.substring(beginning.startPos);

  // Don't complete on just an empty string.
  if (completionPart.trim() == "") {
    return null;
  }

  let properties = completionPart.split(".");
  let matchProp;
  if (properties.length > 1) {
    matchProp = properties.pop().trimLeft();
    for (let i = 0; i < properties.length; i++) {
      let prop = properties[i].trim();
      if (!prop) {
        return null;
      }

      // If obj is undefined or null, then there is no chance to run completion
      // on it. Exit here.
      if (typeof obj === "undefined" || obj === null) {
        return null;
      }

      // Check if prop is a getter function on obj. Functions can change other
      // stuff so we can't execute them to get the next object. Stop here.
      if (WCU.isNonNativeGetter(obj, prop)) {
        return null;
      }
      try {
        obj = obj[prop];
      }
      catch (ex) {
        return null;
      }
    }
  }
  else {
    matchProp = properties[0].trimLeft();
  }

  // If obj is undefined or null, then there is no chance to run
  // completion on it. Exit here.
  if (typeof obj === "undefined" || obj === null) {
    return null;
  }

  // Skip Iterators and Generators.
  if (WCU.isIteratorOrGenerator(obj)) {
    return null;
  }

  let matches = [];
  for (let prop in obj) {
    if (prop.indexOf(matchProp) == 0) {
      matches.push(prop);
    }
  }

  return {
    matchProp: matchProp,
    matches: matches.sort(),
  };
}

return JSPropertyProvider;
})(WebConsoleUtils);
