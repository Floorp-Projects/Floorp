/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const timer = require("sdk/timers");
const { LoaderWithHookedConsole, deactivate, pb, pbUtils } = require("./helper");
const tabs = require("sdk/tabs");
const { getMostRecentBrowserWindow, isWindowPrivate } = require('sdk/window/utils');

exports["test activate private mode via handler"] = function(test) {
  test.waitUntilDone();

  function onReady(tab) {
    if (tab.url == "about:robots")
      tab.close(function() pb.activate());
  }
  function cleanup(tab) {
    if (tab.url == "about:") {
      tabs.removeListener("ready", cleanup);
      tab.close(function onClose() {
        test.done();
      });
    }
  }

  tabs.on("ready", onReady);
  pb.once("start", function onStart() {
    test.pass("private mode was activated");
    pb.deactivate();
  });
  pb.once("stop", function onStop() {
    test.pass("private mode was deactivated");
    tabs.removeListener("ready", onReady);
    tabs.on("ready", cleanup);
  });
  tabs.once("open", function onOpen() {
    tabs.open("about:robots");
  });
  tabs.open("about:");
};

// tests that isActive has the same value as the private browsing service
// expects
exports.testGetIsActive = function (test) {
  test.waitUntilDone();

  test.assertEqual(pb.isActive, false,
                   "private-browsing.isActive is correct without modifying PB service");
  test.assertEqual(pb.isPrivate(), false,
                   "private-browsing.sPrivate() is correct without modifying PB service");

  pb.once("start", function() {
    test.assert(pb.isActive,
                  "private-browsing.isActive is correct after modifying PB service");
    test.assert(pb.isPrivate(),
                  "private-browsing.sPrivate() is correct after modifying PB service");
    // Switch back to normal mode.
    pb.deactivate();
  });
  pb.activate();

  pb.once("stop", function() {
    test.assert(!pb.isActive,
                "private-browsing.isActive is correct after modifying PB service");
    test.assert(!pb.isPrivate(),
                "private-browsing.sPrivate() is correct after modifying PB service");
    test.done();
  });
};

exports.testStart = function(test) {
  test.waitUntilDone();

  pb.on("start", function onStart() {
    test.assertEqual(this, pb, "`this` should be private-browsing module");
    test.assert(pbUtils.getMode(),
                'private mode is active when "start" event is emitted');
    test.assert(pb.isActive,
                '`isActive` is `true` when "start" event is emitted');
    test.assert(pb.isPrivate(),
                '`isPrivate` is `true` when "start" event is emitted');
    pb.removeListener("start", onStart);
    deactivate(function() test.done());
  });
  pb.activate();
};

exports.testStop = function(test) {
  test.waitUntilDone();
  pb.once("stop", function onStop() {
    test.assertEqual(this, pb, "`this` should be private-browsing module");
    test.assertEqual(pbUtils.getMode(), false,
                     "private mode is disabled when stop event is emitted");
    test.assertEqual(pb.isActive, false,
                     "`isActive` is `false` when stop event is emitted");
    test.assertEqual(pb.isPrivate(), false,
                     "`isPrivate()` is `false` when stop event is emitted");
    test.done();
  });
  pb.activate();
  pb.once("start", function() {
    pb.deactivate();
  });
};

exports.testBothListeners = function(test) {
  test.waitUntilDone();
  let stop = false;
  let start = false;

  function onStop() {
    test.assertEqual(stop, false,
                     "stop callback must be called only once");
    test.assertEqual(pbUtils.getMode(), false,
                     "private mode is disabled when stop event is emitted");
    test.assertEqual(pb.isActive, false,
                     "`isActive` is `false` when stop event is emitted");
    test.assertEqual(pb.isPrivate(), false,
                     "`isPrivate()` is `false` when stop event is emitted");

    pb.on("start", finish);
    pb.removeListener("start", onStart);
    pb.removeListener("start", onStart2);
    pb.activate();
    stop = true;
  }

  function onStart() {
    test.assertEqual(false, start,
                     "stop callback must be called only once");
    test.assert(pbUtils.getMode(),
                "private mode is active when start event is emitted");
    test.assert(pb.isActive,
                "`isActive` is `true` when start event is emitted");
    test.assert(pb.isPrivate(),
                "`isPrivate()` is `true` when start event is emitted");

    pb.on("stop", onStop);
    pb.deactivate();
    start = true;
  }

  function onStart2() {
    test.assert(start, "start listener must be called already");
    test.assertEqual(false, stop, "stop callback must not be called yet");
  }

  function finish() {
    test.assert(pbUtils.getMode(), true,
                "private mode is active when start event is emitted");
    test.assert(pb.isActive,
                "`isActive` is `true` when start event is emitted");
    test.assert(pb.isPrivate(),
                "`isPrivate()` is `true` when start event is emitted");

    pb.removeListener("start", finish);
    pb.removeListener("stop", onStop);

    pb.deactivate();
    pb.once("stop", function () {
      test.assertEqual(pbUtils.getMode(), false);
      test.assertEqual(pb.isActive, false);
      test.assertEqual(pb.isPrivate(), false);

      test.done();
    });
  }

  pb.on("start", onStart);
  pb.on("start", onStart2);
  pb.activate();
};

exports.testAutomaticUnload = function(test) {
  test.waitUntilDone();

  // Create another private browsing instance and unload it
  let { loader, errors } = LoaderWithHookedConsole(module);
  let pb2 = loader.require("sdk/private-browsing");
  let called = false;
  pb2.on("start", function onStart() {
    called = true;
    test.fail("should not be called:x");
  });
  loader.unload();

  // Then switch to private mode in order to check that the previous instance
  // is correctly destroyed
  pb.once("start", function onStart() {
    timer.setTimeout(function () {
      test.assert(!called, 
        "First private browsing instance is destroyed and inactive");
      // Must reset to normal mode, so that next test starts with it.
      deactivate(function() {
        test.assert(errors.length, 0, "should have been 1 deprecation error");
        test.done();
      });
    }, 0);
  });

  pb.activate();
};

exports.testUnloadWhileActive = function(test) {
  test.waitUntilDone();

  let called = false;
  let { loader, errors } = LoaderWithHookedConsole(module);
  let pb2 = loader.require("sdk/private-browsing");
  let ul = loader.require("sdk/system/unload");

  let unloadHappened = false;
  ul.when(function() {
    unloadHappened = true;
    timer.setTimeout(function() {
      pb.deactivate();
    });
  });
  pb2.once("start", function() {
    loader.unload();
  });
  pb2.once("stop", function() {
    called = true;
    test.assert(unloadHappened, "the unload event should have already occurred.");
    test.fail("stop should not have been fired");
  });
  pb.once("stop", function() {
    test.assert(!called, "stop was not called on unload");
    test.assert(errors.length, 2, "should have been 2 deprecation errors");
    test.done();
  });

  pb.activate();
};

exports.testIgnoreWindow = function(test) {
  test.waitUntilDone();
  let window = getMostRecentBrowserWindow();

  pb.once('start', function() {
    test.assert(isWindowPrivate(window), 'window is private');
    test.assert(!pbUtils.ignoreWindow(window), 'window is not ignored');
    pb.once('stop', function() {
      test.done();
    });
    pb.deactivate();
  });
  pb.activate();
};
