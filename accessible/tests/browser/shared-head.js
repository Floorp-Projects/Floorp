/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* exported Logger, MOCHITESTS_DIR, invokeSetAttribute, invokeFocus,
            invokeSetStyle, getAccessibleDOMNodeID,
            CURRENT_CONTENT_DIR, loadScripts, loadFrameScripts, Cc, Cu */

const { interfaces: Ci, utils: Cu, classes: Cc } = Components;

/**
 * Current browser test directory path used to load subscripts.
 */
const CURRENT_DIR =
  'chrome://mochitests/content/browser/accessible/tests/browser/';
/**
 * A11y mochitest directory where we find common files used in both browser and
 * plain tests.
 */
const MOCHITESTS_DIR =
  'chrome://mochitests/content/a11y/accessible/tests/mochitest/';
/**
 * A base URL for test files used in content.
 */
const CURRENT_CONTENT_DIR =
  'http://example.com/browser/accessible/tests/browser/';

/**
 * Used to dump debug information.
 */
let Logger = {
  /**
   * Set up this variable to dump log messages into console.
   */
  dumpToConsole: false,

  /**
   * Set up this variable to dump log messages into error console.
   */
  dumpToAppConsole: false,

  /**
   * Return true if dump is enabled.
   */
  get enabled() {
    return this.dumpToConsole || this.dumpToAppConsole;
  },

  /**
   * Dump information into console if applicable.
   */
  log(msg) {
    if (this.enabled) {
      this.logToConsole(msg);
      this.logToAppConsole(msg);
    }
  },

  /**
   * Log message to console.
   */
  logToConsole(msg) {
    if (this.dumpToConsole) {
      dump(`\n${msg}\n`);
    }
  },

  /**
   * Log message to error console.
   */
  logToAppConsole(msg) {
    if (this.dumpToAppConsole) {
      Services.console.logStringMessage(`${msg}`);
    }
  }
};

/**
 * Asynchronously set or remove content element's attribute (in content process
 * if e10s is enabled).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @param  {String}  attr     attribute name
 * @param  {String?} value    optional attribute value, if not present, remove
 *                            attribute
 * @return {Promise}          promise indicating that attribute is set/removed
 */
function invokeSetAttribute(browser, id, attr, value) {
  if (value) {
    Logger.log(`Setting ${attr} attribute to ${value} for node with id: ${id}`);
  } else {
    Logger.log(`Removing ${attr} attribute from node with id: ${id}`);
  }
  return ContentTask.spawn(browser, [id, attr, value],
    ([contentId, contentAttr, contentValue]) => {
      let elm = content.document.getElementById(contentId);
      if (contentValue) {
        elm.setAttribute(contentAttr, contentValue);
      } else {
        elm.removeAttribute(contentAttr);
      }
    });
}

/**
 * Asynchronously set or remove content element's style (in content process if
 * e10s is enabled).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @param  {String}  aStyle   style property name
 * @param  {String?} aValue   optional style property value, if not present,
 *                            remove style
 * @return {Promise}          promise indicating that style is set/removed
 */
function invokeSetStyle(browser, id, style, value) {
  if (value) {
    Logger.log(`Setting ${style} style to ${value} for node with id: ${id}`);
  } else {
    Logger.log(`Removing ${style} style from node with id: ${id}`);
  }
  return ContentTask.spawn(browser, [id, style, value],
    ([contentId, contentStyle, contentValue]) => {
      let elm = content.document.getElementById(contentId);
      if (contentValue) {
        elm.style[contentStyle] = contentValue;
      } else {
        delete elm.style[contentStyle];
      }
    });
}

/**
 * Asynchronously set focus on a content element (in content process if e10s is
 * enabled).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @return {Promise} promise  indicating that focus is set
 */
function invokeFocus(browser, id) {
  Logger.log(`Setting focus on a node with id: ${id}`);
  return ContentTask.spawn(browser, id, contentId => {
    let elm = content.document.getElementById(contentId);
    if (elm instanceof Ci.nsIDOMNSEditableElement && elm.editor ||
        elm instanceof Ci.nsIDOMXULTextBoxElement) {
      elm.selectionStart = elm.selectionEnd = elm.value.length;
    }
    elm.focus();
  });
}

/**
 * Load a list of scripts into the test
 * @param {Array} scripts  a list of scripts to load
 */
function loadScripts(...scripts) {
  for (let script of scripts) {
    let path = typeof script === 'string' ? `${CURRENT_DIR}${script}` :
      `${script.dir}${script.name}`;
    Services.scriptloader.loadSubScript(path, this);
  }
}

/**
 * Load a list of frame scripts into test's content.
 * @param {Object} browser   browser element that content belongs to
 * @param {Array}  scripts   a list of scripts to load into content
 */
function loadFrameScripts(browser, ...scripts) {
  let mm = browser.messageManager;
  for (let script of scripts) {
    let frameScript;
    if (typeof script === 'string') {
      if (script.includes('.js')) {
        // If script string includes a .js extention, assume it is a script
        // path.
        frameScript = `${CURRENT_DIR}${script}`;
      } else {
        // Otherwise it is a serealized script.
        frameScript = `data:,${script}`;
      }
    } else {
      // Script is a object that has { dir, name } format.
      frameScript = `${script.dir}${script.name}`;
    }
    mm.loadFrameScript(frameScript, false, true);
  }
}
