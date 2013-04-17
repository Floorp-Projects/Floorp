/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Loader } = require('sdk/test/loader');
const { browserWindows } = require('sdk/windows');

// TEST: browserWindows Iterator
exports.testBrowserWindowsIterator = function(test) {
  let activeWindowCount = 0;
  let windows = [];
  let i = 0;
  for each (let window in browserWindows) {
    if (window === browserWindows.activeWindow)
      activeWindowCount++;

    test.assertEqual(windows.indexOf(window), -1, 'window not already in iterator');
    test.assertEqual(browserWindows[i++], window, 'browserWindows[x] works');
    windows.push(window);
  }
  test.assertEqual(activeWindowCount, 1, 'activeWindow was found in the iterator');

  i = 0;
  for (let j in browserWindows) {
    test.assertEqual(j, i++, 'for (x in browserWindows) works');
  }
};

exports.testWindowTabsObject_alt = function(test) {
  test.waitUntilDone();

  let window = browserWindows.activeWindow;
  window.tabs.open({
    url: "data:text/html;charset=utf-8,<title>tab 2</title>",
    inBackground: true,
    onReady: function onReady(tab) {
      test.assertEqual(tab.title, "tab 2", "Correct new tab title");
      test.assertNotEqual(window.tabs.activeTab, tab, "Correct active tab");

      // end test
      tab.close(test.done.bind(test));
    }
  });
};

// TEST: browserWindows.activeWindow
exports.testWindowActivateMethod_simple = function(test) {
  let window = browserWindows.activeWindow;
  let tab = window.tabs.activeTab;

  window.activate();

  test.assertEqual(browserWindows.activeWindow, window,
                   "Active window is active after window.activate() call");
  test.assertEqual(window.tabs.activeTab, tab,
                   "Active tab is active after window.activate() call");
  
};
