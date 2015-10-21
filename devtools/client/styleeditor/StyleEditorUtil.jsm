/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "_",
  "assert",
  "log",
  "text",
  "wire",
  "showFilePicker"
];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const PROPERTIES_URL = "chrome://browser/locale/devtools/styleeditor.properties";

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const console = require("resource://gre/modules/Console.jsm").console;
const gStringBundle = Services.strings.createBundle(PROPERTIES_URL);


/**
 * Returns a localized string with the given key name from the string bundle.
 *
 * @param aName
 * @param ...rest
 *        Optional arguments to format in the string.
 * @return string
 */
this._ = function _(aName)
{
  try {
    if (arguments.length == 1) {
      return gStringBundle.GetStringFromName(aName);
    }
    let rest = Array.prototype.slice.call(arguments, 1);
    return gStringBundle.formatStringFromName(aName, rest, rest.length);
  }
  catch (ex) {
    console.error(ex);
    throw new Error("L10N error. '" + aName + "' is missing from " + PROPERTIES_URL);
  }
}

/**
 * Assert an expression is true or throw if false.
 *
 * @param aExpression
 * @param aMessage
 *        Optional message.
 * @return aExpression
 */
this.assert = function assert(aExpression, aMessage)
{
  if (!!!(aExpression)) {
    let msg = aMessage ? "ASSERTION FAILURE:" + aMessage : "ASSERTION FAILURE";
    log(msg);
    throw new Error(msg);
  }
  return aExpression;
}

/**
 * Retrieve or set the text content of an element.
 *
 * @param DOMElement aRoot
 *        The element to use for querySelector.
 * @param string aSelector
 *        Selector string for the element to get/set the text content.
 * @param string aText
 *        Optional text to set.
 * @return string
 *         Text content of matching element or null if there were no element
 *         matching aSelector.
 */
this.text = function text(aRoot, aSelector, aText)
{
  let element = aRoot.querySelector(aSelector);
  if (!element) {
    return null;
  }

  if (aText === undefined) {
    return element.textContent;
  }
  element.textContent = aText;
  return aText;
}

/**
 * Iterates _own_ properties of an object.
 *
 * @param aObject
 *        The object to iterate.
 * @param function aCallback(aKey, aValue)
 */
function forEach(aObject, aCallback)
{
  for (let key in aObject) {
    if (aObject.hasOwnProperty(key)) {
      aCallback(key, aObject[key]);
    }
  }
}

/**
 * Log a message to the console.
 *
 * @param ...rest
 *        One or multiple arguments to log.
 *        If multiple arguments are given, they will be joined by " " in the log.
 */
this.log = function log()
{
  console.logStringMessage(Array.prototype.slice.call(arguments).join(" "));
}

/**
 * Wire up element(s) matching selector with attributes, event listeners, etc.
 *
 * @param DOMElement aRoot
 *        The element to use for querySelectorAll.
 *        Can be null if aSelector is a DOMElement.
 * @param string|DOMElement aSelectorOrElement
 *        Selector string or DOMElement for the element(s) to wire up.
 * @param object aDescriptor
 *        An object describing how to wire matching selector, supported properties
 *        are "events" and "attributes" taking objects themselves.
 *        Each key of properties above represents the name of the event or
 *        attribute, with the value being a function used as an event handler or
 *        string to use as attribute value.
 *        If aDescriptor is a function, the argument is equivalent to :
 *        {events: {'click': aDescriptor}}
 */
this.wire = function wire(aRoot, aSelectorOrElement, aDescriptor)
{
  let matches;
  if (typeof(aSelectorOrElement) == "string") { // selector
    matches = aRoot.querySelectorAll(aSelectorOrElement);
    if (!matches.length) {
      return;
    }
  } else {
    matches = [aSelectorOrElement]; // element
  }

  if (typeof(aDescriptor) == "function") {
    aDescriptor = {events: {click: aDescriptor}};
  }

  for (let i = 0; i < matches.length; i++) {
    let element = matches[i];
    forEach(aDescriptor.events, function (aName, aHandler) {
      element.addEventListener(aName, aHandler, false);
    });
    forEach(aDescriptor.attributes, element.setAttribute);
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
this.showFilePicker = function showFilePicker(path, toSave, parentWindow,
                                              callback, suggestedFilename)
{
  if (typeof(path) == "string") {
    try {
      if (Services.io.extractScheme(path) == "file") {
        let uri = Services.io.newURI(path, null, null);
        let file = uri.QueryInterface(Ci.nsIFileURL).file;
        callback(file);
        return;
      }
    } catch (ex) {
      callback(null);
      return;
    }
    try {
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.initWithPath(path);
      callback(file);
      return;
    } catch (ex) {
      callback(null);
      return;
    }
  }
  if (path) { // "path" is an nsIFile
    callback(path);
    return;
  }

  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let mode = toSave ? fp.modeSave : fp.modeOpen;
  let key = toSave ? "saveStyleSheet" : "importStyleSheet";
  let fpCallback = function(result) {
    if (result == Ci.nsIFilePicker.returnCancel) {
      callback(null);
    } else {
      callback(fp.file);
    }
  };

  if (toSave && suggestedFilename) {
    fp.defaultString = suggestedFilename;
  }

  fp.init(parentWindow, _(key + ".title"), mode);
  fp.appendFilters(_(key + ".filter"), "*.css");
  fp.appendFilters(fp.filterAll);
  fp.open(fpCallback);
  return;
}
