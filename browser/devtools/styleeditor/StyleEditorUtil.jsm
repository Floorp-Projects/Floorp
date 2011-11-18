/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Style Editor code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Cedric Vivier <cedricv@neonux.com> (original author)
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

