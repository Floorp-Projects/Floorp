/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* All top-level definitions here are exports.  */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

this.EXPORTED_SYMBOLS = [
  "getString",
  "assert",
  "log",
  "text",
  "wire",
  "showFilePicker"
];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const PROPERTIES_URL = "chrome://devtools/locale/styleeditor.properties";

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const console = require("resource://gre/modules/Console.jsm").console;
const gStringBundle = Services.strings.createBundle(PROPERTIES_URL);

/**
 * Returns a localized string with the given key name from the string bundle.
 *
 * @param name
 * @param ...rest
 *        Optional arguments to format in the string.
 * @return string
 */
function getString(name) {
  try {
    if (arguments.length == 1) {
      return gStringBundle.GetStringFromName(name);
    }
    let rest = Array.prototype.slice.call(arguments, 1);
    return gStringBundle.formatStringFromName(name, rest, rest.length);
  } catch (ex) {
    console.error(ex);
    throw new Error("L10N error. '" + name + "' is missing from " +
                    PROPERTIES_URL);
  }
}

/**
 * Assert an expression is true or throw if false.
 *
 * @param expression
 * @param message
 *        Optional message.
 * @return expression
 */
function assert(expression, message) {
  if (!expression) {
    let msg = message ? "ASSERTION FAILURE:" + message : "ASSERTION FAILURE";
    log(msg);
    throw new Error(msg);
  }
  return expression;
}

/**
 * Retrieve or set the text content of an element.
 *
 * @param DOMElement root
 *        The element to use for querySelector.
 * @param string selector
 *        Selector string for the element to get/set the text content.
 * @param string textContent
 *        Optional text to set.
 * @return string
 *         Text content of matching element or null if there were no element
 *         matching selector.
 */
function text(root, selector, textContent) {
  let element = root.querySelector(selector);
  if (!element) {
    return null;
  }

  if (textContent === undefined) {
    return element.textContent;
  }
  element.textContent = textContent;
  return textContent;
}

/**
 * Iterates _own_ properties of an object.
 *
 * @param object
 *        The object to iterate.
 * @param function callback(aKey, aValue)
 */
function forEach(object, callback) {
  for (let key in object) {
    if (object.hasOwnProperty(key)) {
      callback(key, object[key]);
    }
  }
}

/**
 * Log a message to the console.
 *
 * @param ...rest
 *        One or multiple arguments to log.
 *        If multiple arguments are given, they will be joined by " "
 *        in the log.
 */
function log() {
  console.logStringMessage(Array.prototype.slice.call(arguments).join(" "));
}

/**
 * Wire up element(s) matching selector with attributes, event listeners, etc.
 *
 * @param DOMElement root
 *        The element to use for querySelectorAll.
 *        Can be null if selector is a DOMElement.
 * @param string|DOMElement selectorOrElement
 *        Selector string or DOMElement for the element(s) to wire up.
 * @param object descriptor
 *        An object describing how to wire matching selector,
 *        supported properties are "events" and "attributes" taking
 *        objects themselves.
 *        Each key of properties above represents the name of the event or
 *        attribute, with the value being a function used as an event handler or
 *        string to use as attribute value.
 *        If descriptor is a function, the argument is equivalent to :
 *        {events: {'click': descriptor}}
 */
function wire(root, selectorOrElement, descriptor) {
  let matches;
  if (typeof selectorOrElement == "string") {
    // selector
    matches = root.querySelectorAll(selectorOrElement);
    if (!matches.length) {
      return;
    }
  } else {
    // element
    matches = [selectorOrElement];
  }

  if (typeof descriptor == "function") {
    descriptor = {events: {click: descriptor}};
  }

  for (let i = 0; i < matches.length; i++) {
    let element = matches[i];
    forEach(descriptor.events, function (name, handler) {
      element.addEventListener(name, handler, false);
    });
    forEach(descriptor.attributes, element.setAttribute);
  }
}

/**
 * Show file picker and return the file user selected.
 *
 * @param mixed file
 *        Optional nsIFile or string representing the filename to auto-select.
 * @param boolean toSave
 *        If true, the user is selecting a filename to save.
 * @param nsIWindow parentWindow
 *        Optional parent window. If null the parent window of the file picker
 *        will be the window of the attached input element.
 * @param callback
 *        The callback method, which will be called passing in the selected
 *        file or null if the user did not pick one.
 * @param AString suggestedFilename
 *        The suggested filename when toSave is true.
 */
function showFilePicker(path, toSave, parentWindow, callback,
                        suggestedFilename) {
  if (typeof path == "string") {
    try {
      if (Services.io.extractScheme(path) == "file") {
        let uri = Services.io.newURI(path);
        let file = uri.QueryInterface(Ci.nsIFileURL).file;
        callback(file);
        return;
      }
    } catch (ex) {
      callback(null);
      return;
    }
    try {
      let file =
          Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.initWithPath(path);
      callback(file);
      return;
    } catch (ex) {
      callback(null);
      return;
    }
  }
  if (path) {
    // "path" is an nsIFile
    callback(path);
    return;
  }

  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let mode = toSave ? fp.modeSave : fp.modeOpen;
  let key = toSave ? "saveStyleSheet" : "importStyleSheet";
  let fpCallback = function (result) {
    if (result == Ci.nsIFilePicker.returnCancel) {
      callback(null);
    } else {
      callback(fp.file);
    }
  };

  if (toSave && suggestedFilename) {
    fp.defaultString = suggestedFilename;
  }

  fp.init(parentWindow, getString(key + ".title"), mode);
  fp.appendFilter(getString(key + ".filter"), "*.css");
  fp.appendFilters(fp.filterAll);
  fp.open(fpCallback);
}
