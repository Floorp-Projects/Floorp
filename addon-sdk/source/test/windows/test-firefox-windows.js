/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci } = require('chrome');
const { setTimeout } = require('sdk/timers');
const { Loader } = require('sdk/test/loader');
const { onFocus, getMostRecentWindow, windows } = require('sdk/window/utils');
const { open, close, focus } = require('sdk/window/helpers');
const { browserWindows } = require("sdk/windows");
const tabs = require("sdk/tabs");
const winUtils = require("sdk/deprecated/window-utils");
const { WindowTracker } = winUtils;
const { isPrivate } = require('sdk/private-browsing');
const { isWindowPBSupported } = require('sdk/private-browsing/utils');

// TEST: open & close window
exports.testOpenAndCloseWindow = function(test) {
  test.waitUntilDone();

  test.assertEqual(browserWindows.length, 1, "Only one window open");

  browserWindows.open({
    url: "data:text/html;charset=utf-8,<title>windows API test</title>",
    onOpen: function(window) {
      test.assertEqual(this, browserWindows,
                       "The 'this' object is the windows object.");
      test.assertEqual(window.tabs.length, 1, "Only one tab open");
      test.assertEqual(browserWindows.length, 2, "Two windows open");
      window.tabs.activeTab.on('ready', function onReady(tab) {
        tab.removeListener('ready', onReady);
        test.assert(window.title.indexOf("windows API test") != -1,
                    "URL correctly loaded");
        window.close();
      });
    },
    onClose: function(window) {
      test.assertEqual(window.tabs.length, 0, "Tabs were cleared");
      test.assertEqual(browserWindows.length, 1, "Only one window open");
      test.done();
    }
  });
};

exports.testAutomaticDestroy = function(test) {
  test.waitUntilDone();

  let windows = browserWindows;

  // Create a second windows instance that we will unload
  let called = false;
  let loader = Loader(module);
  let windows2 = loader.require("sdk/windows").browserWindows;
  windows2.on("open", function() {
    called = true;
  });

  loader.unload();

  // Fire a windows event and check that this unloaded instance is inactive
  windows.open({
    url: "data:text/html;charset=utf-8,foo",
    onOpen: function(window) {
      setTimeout(function () {
        test.assert(!called,
          "Unloaded windows instance is destroyed and inactive");
        window.close(function () {
          test.done();
        });
      });
    }
  });
};

exports.testWindowTabsObject = function(test) {
  test.waitUntilDone();

  let count = 0;
  let window;
  function runTest() {
    if (++count != 2)
      return;

    test.assertEqual(window.tabs.length, 1, "Only 1 tab open");
    test.assertEqual(window.tabs.activeTab.title, "tab 1", "Correct active tab");

    window.tabs.open({
      url: "data:text/html;charset=utf-8,<title>tab 2</title>",
      inBackground: true,
      onReady: function onReady(newTab) {
        test.assertEqual(window.tabs.length, 2, "New tab open");
        test.assertEqual(newTab.title, "tab 2", "Correct new tab title");
        test.assertEqual(window.tabs.activeTab.title, "tab 1", "Correct active tab");

        let i = 1;
        for each (let tab in window.tabs)
          test.assertEqual(tab.title, "tab " + i++, "Correct title");

        window.close();
      }
    });
  }
  browserWindows.open({
    url: "data:text/html;charset=utf-8,<title>tab 1</title>",
    onActivate: function onActivate(win) {
      window = win;
      runTest();
    },
    onClose: function onClose(window) {
      test.assertEqual(window.tabs.length, 0, "No more tabs on closed window");
      test.done();
    }
  });
  tabs.once("ready", runTest);
};

exports.testOnOpenOnCloseListeners = function(test) {
  test.waitUntilDone();
  let windows = browserWindows;

  test.assertEqual(browserWindows.length, 1, "Only one window open");

  let received = {
    listener1: false,
    listener2: false,
    listener3: false,
    listener4: false
  }

   function listener1() {
    test.assertEqual(this, windows, "The 'this' object is the windows object.");
    if (received.listener1)
      test.fail("Event received twice");
    received.listener1 = true;
  }

  function listener2() {
    if (received.listener2)
      test.fail("Event received twice");
    received.listener2 = true;
  }

  function listener3() {
    test.assertEqual(this, windows, "The 'this' object is the windows object.");
    if (received.listener3)
      test.fail("Event received twice");
    received.listener3 = true;
  }

  function listener4() {
    if (received.listener4)
      test.fail("Event received twice");
    received.listener4 = true;
  }

  windows.on('open', listener1);
  windows.on('open', listener2);
  windows.on('close', listener3);
  windows.on('close', listener4);

  function verify() {
    test.assert(received.listener1, "onOpen handler called");
    test.assert(received.listener2, "onOpen handler called");
    test.assert(received.listener3, "onClose handler called");
    test.assert(received.listener4, "onClose handler called");

    windows.removeListener('open', listener1);
    windows.removeListener('open', listener2);
    windows.removeListener('close', listener3);
    windows.removeListener('close', listener4);
    test.done();
  }


  windows.open({
    url: "data:text/html;charset=utf-8,foo",
    onOpen: function(window) {
      window.close(verify);
    }
  });
};

exports.testActiveWindow = function(test) {
  const xulApp = require("sdk/system/xul-app");
  if (xulApp.versionInRange(xulApp.platformVersion, "1.9.2", "1.9.2.*")) {
    test.pass("This test is disabled on 3.6. For more information, see bug 598525");
    return;
  }

  let windows = browserWindows;

  // API window objects
  let window2, window3;

  // Raw window objects
  let rawWindow2, rawWindow3;

  test.waitUntilDone();

  let testSteps = [
    function() {
      test.assertEqual(windows.length, 3, "Correct number of browser windows");
      let count = 0;
      for (let window in windows)
        count++;
      test.assertEqual(count, 3, "Correct number of windows returned by iterator");

      test.assertEqual(windows.activeWindow.title, window3.title, "Correct active window - 3");

      continueAfterFocus(rawWindow2);
      rawWindow2.focus();
    },
    function() {
      nextStep();
    },
    function() {
      test.assertEqual(windows.activeWindow.title, window2.title, "Correct active window - 2");

      continueAfterFocus(rawWindow2);
      window2.activate();
    },
    function() {
      test.assertEqual(windows.activeWindow.title, window2.title, "Correct active window - 2");
      continueAfterFocus(rawWindow3);
      window3.activate();
    },
    function() {
      test.assertEqual(windows.activeWindow.title, window3.title, "Correct active window - 3");
      finishTest();
    }
  ];

  let newWindow = null;
  let tracker = new WindowTracker({
    onTrack: function(window) {
      newWindow = window;
    }
  });

  windows.open({
    url: "data:text/html;charset=utf-8,<title>window 2</title>",
    onOpen: function(window) {
      window.tabs.activeTab.on('ready', function() {
        window2 = window;
        test.assert(newWindow, "A new window was opened");
        rawWindow2 = newWindow;
        newWindow = null;
        test.assertEqual(rawWindow2.content.document.title, "window 2", "Got correct raw window 2");
        test.assertEqual(rawWindow2.document.title, window2.title, "Saw correct title on window 2");

        windows.open({
          url: "data:text/html;charset=utf-8,<title>window 3</title>",
          onOpen: function(window) {
            window.tabs.activeTab.on('ready', function onReady() {
              window3 = window;
              test.assert(newWindow, "A new window was opened");
              rawWindow3 = newWindow;
              tracker.unload();
              test.assertEqual(rawWindow3.content.document.title, "window 3", "Got correct raw window 3");
              test.assertEqual(rawWindow3.document.title, window3.title, "Saw correct title on window 3");
              continueAfterFocus(rawWindow3);
              rawWindow3.focus();
            });
          }
        });
      });
    }
  });

  function nextStep() {
    if (testSteps.length)
      testSteps.shift()();
  }

  let continueAfterFocus = function(w) onFocus(w).then(nextStep);

  function finishTest() {
    window3.close(function() {
      window2.close(function() {
        test.done();
      });
    });
  }
};

exports.testTrackWindows = function(test) {
  test.waitUntilDone();

  let windows = [];
  let actions = [];

  let expects = [
    "activate 0", "global activate 0", "deactivate 0", "global deactivate 0",
    "activate 1", "global activate 1", "deactivate 1", "global deactivate 1",
    "activate 2", "global activate 2"
  ];

  function openWindow() {
    windows.push(browserWindows.open({
      url: "data:text/html;charset=utf-8,<i>testTrackWindows</i>",

      onActivate: function(window) {
        let index = windows.indexOf(window);

        test.assertEqual(actions.join(), expects.slice(0, index*4).join(), expects[index*4]);
        actions.push("activate " + index);

        if (windows.length < 3) {
          openWindow()
        }
        else {
          let count = windows.length;
          for each (let win in windows) {
            win.close(function() {
              if (--count == 0) {
                test.done();
              }
            });
          }
        }
      },

      onDeactivate: function(window) {
        let index = windows.indexOf(window);

        test.assertEqual(actions.join(), expects.slice(0, index*4 + 2).join(), expects[index*4 + 2]);
        actions.push("deactivate " + index)
      }
    }));
  }

  browserWindows.on("activate", function (window) {
    let index = windows.indexOf(window);
    // only concerned with windows opened for this test
    if (index < 0)
      return;

    test.assertEqual(actions.join(), expects.slice(0, index*4 + 1).join(), expects[index*4 + 1]);
    actions.push("global activate " + index)
  })

  browserWindows.on("deactivate", function (window) {
    let index = windows.indexOf(window);
    // only concerned with windows opened for this test
    if (index < 0)
      return;

    test.assertEqual(actions.join(), expects.slice(0, index*4 + 3).join(), expects[index*4 + 3]);
    actions.push("global deactivate " + index)
  })

  openWindow();
}

// test that it is not possible to open a private window by default
exports.testWindowOpenPrivateDefault = function(test) {
  test.waitUntilDone();

  browserWindows.open({
    url: 'about:mozilla',
    isPrivate: true,
    onOpen: function(window) {
      let tab = window.tabs[0];
      tab.once('ready', function() {
        test.assertEqual(tab.url, 'about:mozilla', 'opened correct tab');
        test.assertEqual(isPrivate(tab), false, 'tab is not private');

        window.close(test.done.bind(test));
      });
    }
  });
}

// test that it is not possible to find a private window in
// windows module's iterator
exports.testWindowIteratorPrivateDefault = function(test) {
  test.waitUntilDone();

  test.assertEqual(browserWindows.length, 1, 'only one window open');

  open('chrome://browser/content/browser.xul', {
    features: {
      private: true,
      chrome: true
    }
  }).then(function(window) focus(window).then(function() {
    // test that there is a private window opened
    test.assertEqual(isPrivate(window), isWindowPBSupported, 'there is a private window open');
    test.assertStrictEqual(window, winUtils.activeWindow);
    test.assertStrictEqual(window, getMostRecentWindow());

    test.assert(!isPrivate(browserWindows.activeWindow));

    if (isWindowPBSupported) {
      test.assertEqual(browserWindows.length, 1, 'only one window in browserWindows');
      test.assertEqual(windows().length, 1, 'only one window in windows()');
    }
    else {
      test.assertEqual(browserWindows.length, 2, 'two windows open');
      test.assertEqual(windows().length, 2, 'two windows in windows()');
    }
    test.assertEqual(windows(null, { includePrivate: true }).length, 2);

    for each(let window in browserWindows) {
      // test that all windows in iterator are not private
      test.assert(!isPrivate(window), 'no window in browserWindows is private');
    }

    close(window).then(test.done.bind(test));
  }));
}
