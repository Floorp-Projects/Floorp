/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* import-globals-from ../mochitest/common.js */
/* import-globals-from events.js */

/* exported Logger, MOCHITESTS_DIR, invokeSetAttribute, invokeFocus,
            invokeSetStyle, getAccessibleDOMNodeID,
            addAccessibleTask, findAccessibleChildByID, isDefunct,
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

/**
 * A wrapper around browser test add_task that triggers an accessible test task
 * as a new browser test task with given document, data URL or markup snippet.
 * @param  {String}                 doc  URL (relative to current directory) or
 *                                       data URL or markup snippet that is used
 *                                       to test content with
 * @param  {Function|AsyncFunction} task a generator or a function with tests to
 *                                       run
 */
function addAccessibleTask(doc, task) {
  add_task(async function() {
    let url;
    if (doc.includes('doc_')) {
      url = `${CURRENT_CONTENT_DIR}e10s/${doc}`;
    } else {
      // Assume it's a markup snippet.
      url = "data:text/html;charset=utf-8;base64,";
      url += btoa(
        `<html>
          <head>
            <meta charset="utf-8"/>
            <title>Accessibility Test</title>
          </head>
          <body id="body">${doc}</body>
        </html>`);
    }

    registerCleanupFunction(() => {
      let observers = Services.obs.enumerateObservers('accessible-event');
      while (observers.hasMoreElements()) {
        Services.obs.removeObserver(
          observers.getNext().QueryInterface(Ci.nsIObserver),
          'accessible-event');
      }
    });

    let onDocLoad = waitForEvent(EVENT_DOCUMENT_LOAD_COMPLETE, 'body');

    await BrowserTestUtils.withNewTab({
      gBrowser,
      url: url
    }, async function(browser) {
      registerCleanupFunction(() => {
        if (browser) {
          let tab = gBrowser.getTabForBrowser(browser);
          if (tab && !tab.closing && tab.linkedBrowser) {
            gBrowser.removeTab(tab);
          }
        }
      });

      await SimpleTest.promiseFocus(browser);

      loadFrameScripts(browser,
        'let { document, window, navigator } = content;',
        { name: 'common.js', dir: MOCHITESTS_DIR });

      Logger.log(
        `e10s enabled: ${Services.appinfo.browserTabsRemoteAutostart}`);
      Logger.log(`Actually remote browser: ${browser.isRemoteBrowser}`);

      let event = await onDocLoad;
      await task(browser, event.accessible);
    });
  });
}

/**
 * Check if an accessible object has a defunct test.
 * @param  {nsIAccessible}  accessible object to test defunct state for
 * @return {Boolean}        flag indicating defunct state
 */
function isDefunct(accessible) {
  let defunct = false;
  try {
    let extState = {};
    accessible.getState({}, extState);
    defunct = extState.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT;
  } catch (x) {
    defunct = true;
  } finally {
    if (defunct) {
      Logger.log(`Defunct accessible: ${prettyName(accessible)}`);
    }
  }
  return defunct;
}

/**
 * Traverses the accessible tree starting from a given accessible as a root and
 * looks for an accessible that matches based on its DOMNode id.
 * @param  {nsIAccessible}  accessible root accessible
 * @param  {String}         id         id to look up accessible for
 * @param  {Array?}         interfaces the interface or an array interfaces
 *                                     to query it/them from obtained accessible
 * @return {nsIAccessible?}            found accessible if any
 */
function findAccessibleChildByID(accessible, id, interfaces) {
  if (getAccessibleDOMNodeID(accessible) === id) {
    return queryInterfaces(accessible, interfaces);
  }
  for (let i = 0; i < accessible.children.length; ++i) {
    let found = findAccessibleChildByID(accessible.getChildAt(i), id);
    if (found) {
      return queryInterfaces(found, interfaces);
    }
  }
}

function queryInterfaces(accessible, interfaces) {
  if (!interfaces) {
    return accessible;
  }

  for (let iface of interfaces.filter(i => !(accessible instanceof i))) {
    try {
      accessible.QueryInterface(iface);
    } catch (e) {
      ok(false, "Can't query " + iface);
    }
  }

  return accessible;
}
