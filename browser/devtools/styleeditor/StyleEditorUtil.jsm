/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "_",
  "assert",
  "attr",
  "getCurrentBrowserTabContentWindow",
  "log",
  "text",
  "wire"
];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const PROPERTIES_URL = "chrome://browser/locale/devtools/styleeditor.properties";

const console = Services.console;
const gStringBundle = Services.strings.createBundle(PROPERTIES_URL);


/**
 * Returns a localized string with the given key name from the string bundle.
 *
 * @param aName
 * @param ...rest
 *        Optional arguments to format in the string.
 * @return string
 */
function _(aName)
{

  if (arguments.length == 1) {
    return gStringBundle.GetStringFromName(aName);
  }
  let rest = Array.prototype.slice.call(arguments, 1);
  return gStringBundle.formatStringFromName(aName, rest, rest.length);
}

/**
 * Assert an expression is true or throw if false.
 *
 * @param aExpression
 * @param aMessage
 *        Optional message.
 * @return aExpression
 */
function assert(aExpression, aMessage)
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
function text(aRoot, aSelector, aText)
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
function log()
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
 *        are "events", "attributes" and "userData" taking objects themselves.
 *        Each key of properties above represents the name of the event, attribute
 *        or userData, with the value being a function used as an event handler,
 *        string to use as attribute value, or object to use as named userData
 *        respectively.
 *        If aDescriptor is a function, the argument is equivalent to :
 *        {events: {'click': aDescriptor}}
 */
function wire(aRoot, aSelectorOrElement, aDescriptor)
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

  for (let i = 0; i < matches.length; ++i) {
    let element = matches[i];
    forEach(aDescriptor.events, function (aName, aHandler) {
      element.addEventListener(aName, aHandler, false);
    });
    forEach(aDescriptor.attributes, element.setAttribute);
    forEach(aDescriptor.userData, element.setUserData);
  }
}

