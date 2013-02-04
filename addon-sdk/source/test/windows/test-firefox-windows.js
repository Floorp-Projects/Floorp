/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci } = require('chrome');
const { setTimeout } = require('sdk/timers');
const { Loader } = require('sdk/test/loader');
const wm = Cc['@mozilla.org/appshell/window-mediator;1'].
           getService(Ci.nsIWindowMediator);

const { browserWindows } = require("sdk/windows");
const tabs = require("tabs");

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
    onActivate: function onOpen(win) {
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

      rawWindow2.focus();
      continueAfterFocus(rawWindow2);
    },
    function() {
      nextStep();
    },
    function() {
      /**
       * Bug 614079: This test fails intermittently on some specific linux
       *             environnements, without being able to reproduce it in same
       *             distribution with same window manager.
       *             Disable it until being able to reproduce it easily.

      // On linux, focus is not consistent, so we can't be sure
      // what window will be on top.
      // Here when we focus "non-browser" window,
      // Any Browser window may be selected as "active".
      test.assert(windows.activeWindow == window2 || windows.activeWindow == window3,
        "Non-browser windows aren't handled by this module");
      */
      window2.activate();
      continueAfterFocus(rawWindow2);
    },
    function() {
      test.assertEqual(windows.activeWindow.title, window2.title, "Correct active window - 2");
      window3.activate();
      continueAfterFocus(rawWindow3);
    },
    function() {
      test.assertEqual(windows.activeWindow.title, window3.title, "Correct active window - 3");
      finishTest();
    }
  ];

  windows.open({
    url: "data:text/html;charset=utf-8,<title>window 2</title>",
    onOpen: function(window) {
      window2 = window;
      rawWindow2 = wm.getMostRecentWindow("navigator:browser");

      windows.open({
        url: "data:text/html;charset=utf-8,<title>window 3</title>",
        onOpen: function(window) {
          window.tabs.activeTab.on('ready', function onReady() {
            window3 = window;
            rawWindow3 = wm.getMostRecentWindow("navigator:browser");
            nextStep()
          });
        }
      });
    }
  });

  function nextStep() {
    if (testSteps.length > 0)
      testSteps.shift()();
  }

  function continueAfterFocus(targetWindow) {
    // Based on SimpleTest.waitForFocus
    var fm = Cc["@mozilla.org/focus-manager;1"].
             getService(Ci.nsIFocusManager);

    var childTargetWindow = {};
    fm.getFocusedElementForWindow(targetWindow, true, childTargetWindow);
    childTargetWindow = childTargetWindow.value;

    var focusedChildWindow = {};
    if (fm.activeWindow) {
      fm.getFocusedElementForWindow(fm.activeWindow, true, focusedChildWindow);
      focusedChildWindow = focusedChildWindow.value;
    }

    var focused = (focusedChildWindow == childTargetWindow);
    if (focused) {
      nextStep();
    } else {
      childTargetWindow.addEventListener("focus", function focusListener() {
        childTargetWindow.removeEventListener("focus", focusListener, true);
        nextStep();
      }, true);
    }

  }

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

  function shutdown(window) {
    if (this.length === 1) {
      test.assertEqual(actions.join(), expects.join(),
        "correct activate and deactivate sequence")

      test.done();
    }
  }

  function openWindow() {
    windows.push(browserWindows.open({
      url: "data:text/html;charset=utf-8,<i>testTrackWindows</i>",

      onActivate: function(window) {
        let index = windows.indexOf(window);

        actions.push("activate " + index);

        if (windows.length < 3)
          openWindow()
        else
          for each (let win in windows)
            win.close(shutdown)
      },

      onDeactivate: function(window) {
        let index = windows.indexOf(window);

        actions.push("deactivate " + index)
      }
    }));
  }

  browserWindows.on("activate", function (window) {
    let index = windows.indexOf(window);
    // only concerned with windows opened for this test
    if (index < 0)
      return;
    actions.push("global activate " + index)
  })

  browserWindows.on("deactivate", function (window) {
    let index = windows.indexOf(window);
    // only concerned with windows opened for this test
    if (index < 0)
      return;
    actions.push("global deactivate " + index)
  })

  openWindow();
}
