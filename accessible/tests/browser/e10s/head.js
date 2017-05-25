/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* exported addAccessibleTask, findAccessibleChildByID, isDefunct */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  'chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js',
  this);

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
      url = `data:text/html,
        <html>
          <head>
            <meta charset="utf-8"/>
            <title>Accessibility Test</title>
          </head>
          <body id="body">${doc}</body>
        </html>`;
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
 * @return {nsIAccessible?}            found accessible if any
 */
function findAccessibleChildByID(accessible, id) {
  if (getAccessibleDOMNodeID(accessible) === id) {
    return accessible;
  }
  for (let i = 0; i < accessible.children.length; ++i) {
    let found = findAccessibleChildByID(accessible.getChildAt(i), id);
    if (found) {
      return found;
    }
  }
}

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as events.js.
/* import-globals-from ../../mochitest/common.js */
/* import-globals-from events.js */
loadScripts({ name: 'common.js', dir: MOCHITESTS_DIR }, 'e10s/events.js');
