/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {parseDeclarations} = require("devtools/shared/css-parsing-utils");
const promise = require("promise");
const {getCSSLexer} = require("devtools/shared/css-lexer");
const {KeyCodes} = require("devtools/client/shared/keycodes");

const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Create a child element with a set of attributes.
 *
 * @param {Element} parent
 *        The parent node.
 * @param {string} tagName
 *        The tag name.
 * @param {object} attributes
 *        A set of attributes to set on the node.
 */
function createChild(parent, tagName, attributes = {}) {
  let elt = parent.ownerDocument.createElementNS(HTML_NS, tagName);
  for (let attr in attributes) {
    if (attributes.hasOwnProperty(attr)) {
      if (attr === "textContent") {
        elt.textContent = attributes[attr];
      } else if (attr === "child") {
        elt.appendChild(attributes[attr]);
      } else {
        elt.setAttribute(attr, attributes[attr]);
      }
    }
  }
  parent.appendChild(elt);
  return elt;
}

exports.createChild = createChild;

/**
 * Append a text node to an element.
 *
 * @param {Element} parent
 *        The parent node.
 * @param {string} text
 *        The text content for the text node.
 */
function appendText(parent, text) {
  parent.appendChild(parent.ownerDocument.createTextNode(text));
}

exports.appendText = appendText;

/**
 * Called when a character is typed in a value editor.  This decides
 * whether to advance or not, first by checking to see if ";" was
 * typed, and then by lexing the input and seeing whether the ";"
 * would be a terminator at this point.
 *
 * @param {number} keyCode
 *        Key code to be checked.
 * @param {string} aValue
 *        Current text editor value.
 * @param {number} insertionPoint
 *        The index of the insertion point.
 * @return {Boolean} True if the focus should advance; false if
 *        the character should be inserted.
 */
function advanceValidate(keyCode, value, insertionPoint) {
  // Only ";" has special handling here.
  if (keyCode !== KeyCodes.DOM_VK_SEMICOLON) {
    return false;
  }

  // Insert the character provisionally and see what happens.  If we
  // end up with a ";" symbol token, then the semicolon terminates the
  // value.  Otherwise it's been inserted in some spot where it has a
  // valid meaning, like a comment or string.
  value = value.slice(0, insertionPoint) + ";" + value.slice(insertionPoint);
  let lexer = getCSSLexer(value);
  while (true) {
    let token = lexer.nextToken();
    if (token.endOffset > insertionPoint) {
      if (token.tokenType === "symbol" && token.text === ";") {
        // The ";" is a terminator.
        return true;
      }
      // The ";" is not a terminator in this context.
      break;
    }
  }
  return false;
}

exports.advanceValidate = advanceValidate;

/**
 * Create a throttling function wrapper to regulate its frequency.
 *
 * @param {Function} func
 *         The function to throttle
 * @param {number} wait
 *         The throttling period
 * @param {Object} scope
 *         The scope to use for func
 * @return {Function} The throttled function
 */
function throttle(func, wait, scope) {
  let timer = null;

  return function () {
    if (timer) {
      clearTimeout(timer);
    }

    let args = arguments;
    timer = setTimeout(function () {
      timer = null;
      func.apply(scope, args);
    }, wait);
  };
}

exports.throttle = throttle;

/**
 * Event handler that causes a blur on the target if the input has
 * multiple CSS properties as the value.
 */
function blurOnMultipleProperties(cssProperties) {
  return (e) => {
    setTimeout(() => {
      let props = parseDeclarations(cssProperties.isKnown, e.target.value);
      if (props.length > 1) {
        e.target.blur();
      }
    }, 0);
  };
}

exports.blurOnMultipleProperties = blurOnMultipleProperties;

/**
 * Log the provided error to the console and return a rejected Promise for
 * this error.
 *
 * @param {Error} error
 *         The error to log
 * @return {Promise} A rejected promise
 */
function promiseWarn(error) {
  console.error(error);
  return promise.reject(error);
}

exports.promiseWarn = promiseWarn;
