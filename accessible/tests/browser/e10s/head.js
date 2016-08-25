/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_DOCUMENT_LOAD_COMPLETE, CURRENT_CONTENT_DIR, loadFrameScripts */

/* exported addAccessibleTask */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  'chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js',
  this);

/**
 * A wrapper around browser test add_task that triggers an accessible test task
 * as a new browser test task with given document, data URL or markup snippet.
 * @param  {String}             doc    URL (relative to current directory) or
 *                                     data URL or markup snippet that is used
 *                                     to test content with
 * @param  {Function|Function*} task   a generator or a function with tests to
 *                                     run
 */
function addAccessibleTask(doc, task) {
  add_task(function*() {
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

    yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: url
    }, function*(browser) {
      registerCleanupFunction(() => {
        if (browser) {
          let tab = gBrowser.getTabForBrowser(browser);
          if (tab && !tab.closing && tab.linkedBrowser) {
            gBrowser.removeTab(tab);
          }
        }
      });

      yield SimpleTest.promiseFocus(browser);

      loadFrameScripts(browser,
        'let { document, window, navigator } = content;',
        { name: 'common.js', dir: MOCHITESTS_DIR });

      Logger.log(
        `e10s enabled: ${Services.appinfo.browserTabsRemoteAutostart}`);
      Logger.log(`Actually remote browser: ${browser.isRemoteBrowser}`);

      let event = yield onDocLoad;
      yield task(browser, event.accessible);
    });
  });
}

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as events.js.
loadScripts({ name: 'common.js', dir: MOCHITESTS_DIR }, 'e10s/events.js');
