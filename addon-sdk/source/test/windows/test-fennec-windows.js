/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci } = require('chrome');
const { setTimeout } = require('sdk/timers');
const { Loader } = require('sdk/test/loader');
const WM = Cc['@mozilla.org/appshell/window-mediator;1'].
           getService(Ci.nsIWindowMediator);
const { browserWindows } = require('sdk/windows');

const ERR_MSG = 'This method is not yet supported by Fennec, consider using require("tabs") instead';

// TEST: browserWindows.length for Fennec
exports.testBrowserWindowsLength = function(test) {
  test.assertEqual(browserWindows.length, 1, "Only one window open");
};

// TEST: open & close window
exports.testOpenWindow = function(test) {
  let tabCount = browserWindows.activeWindow.tabs.length;
  let url = "data:text/html;charset=utf-8,<title>windows%20API%20test</title>";

  try {
    browserWindows.open({url: url});
    test.fail('Error was not thrown');
  }
  catch(e) {
    test.assertEqual(e.message, ERR_MSG, 'Error is thrown on windows.open');
    test.assertEqual(browserWindows.length, 1, "Only one window open");
  }
};

exports.testCloseWindow = function(test) {
  let window = browserWindows.activeWindow;

  try {
    window.close();
    test.fail('Error was not thrown');
  }
  catch(e) {
    test.assertEqual(e.message, ERR_MSG, 'Error is thrown on windows.close');
    test.assertEqual(browserWindows.length, 1, "Only one window open");
  }
};
