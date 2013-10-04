/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const timer = require("sdk/timers");
const { LoaderWithHookedConsole, deactivate, pb, pbUtils } = require("./helper");
const tabs = require("sdk/tabs");
const { getMostRecentBrowserWindow, isWindowPrivate } = require('sdk/window/utils');
const { set: setPref } = require("sdk/preferences/service");
const DEPRECATE_PREF = "devtools.errorconsole.deprecation_warnings";

exports["test activate private mode via handler"] = function(assert, done) {
  function onReady(tab) {
    if (tab.url == "about:robots")
      tab.close(function() pb.activate());
  }
  function cleanup(tab) {
    if (tab.url == "about:") {
      tabs.removeListener("ready", cleanup);
      tab.close(function onClose() {
        done();
      });
    }
  }

  tabs.on("ready", onReady);
  pb.once("start", function onStart() {
    assert.pass("private mode was activated");
    pb.deactivate();
  });
  pb.once("stop", function onStop() {
    assert.pass("private mode was deactivated");
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
exports.testGetIsActive = function (assert) {
  assert.equal(pb.isActive, false,
                   "private-browsing.isActive is correct without modifying PB service");
  assert.equal(pb.isPrivate(), false,
                   "private-browsing.sPrivate() is correct without modifying PB service");

  pb.once("start", function() {
    assert.ok(pb.isActive,
                  "private-browsing.isActive is correct after modifying PB service");
    assert.ok(pb.isPrivate(),
                  "private-browsing.sPrivate() is correct after modifying PB service");
    // Switch back to normal mode.
    pb.deactivate();
  });
  pb.activate();

  pb.once("stop", function() {
    assert.ok(!pb.isActive,
                "private-browsing.isActive is correct after modifying PB service");
    assert.ok(!pb.isPrivate(),
                "private-browsing.sPrivate() is correct after modifying PB service");
    test.done();
  });
};

exports.testStart = function(assert, done) {
  pb.on("start", function onStart() {
    assert.equal(this, pb, "`this` should be private-browsing module");
    assert.ok(pbUtils.getMode(),
                'private mode is active when "start" event is emitted');
    assert.ok(pb.isActive,
                '`isActive` is `true` when "start" event is emitted');
    assert.ok(pb.isPrivate(),
                '`isPrivate` is `true` when "start" event is emitted');
    pb.removeListener("start", onStart);
    deactivate(done);
  });
  pb.activate();
};

exports.testStop = function(assert, done) {
  pb.once("stop", function onStop() {
    assert.equal(this, pb, "`this` should be private-browsing module");
    assert.equal(pbUtils.getMode(), false,
                     "private mode is disabled when stop event is emitted");
    assert.equal(pb.isActive, false,
                     "`isActive` is `false` when stop event is emitted");
    assert.equal(pb.isPrivate(), false,
                     "`isPrivate()` is `false` when stop event is emitted");
    done();
  });
  pb.activate();
  pb.once("start", function() {
    pb.deactivate();
  });
};

exports.testBothListeners = function(assert, done) {
  let stop = false;
  let start = false;

  function onStop() {
    assert.equal(stop, false,
                     "stop callback must be called only once");
    assert.equal(pbUtils.getMode(), false,
                     "private mode is disabled when stop event is emitted");
    assert.equal(pb.isActive, false,
                     "`isActive` is `false` when stop event is emitted");
    assert.equal(pb.isPrivate(), false,
                     "`isPrivate()` is `false` when stop event is emitted");

    pb.on("start", finish);
    pb.removeListener("start", onStart);
    pb.removeListener("start", onStart2);
    pb.activate();
    stop = true;
  }

  function onStart() {
    assert.equal(false, start,
                     "stop callback must be called only once");
    assert.ok(pbUtils.getMode(),
                "private mode is active when start event is emitted");
    assert.ok(pb.isActive,
                "`isActive` is `true` when start event is emitted");
    assert.ok(pb.isPrivate(),
                "`isPrivate()` is `true` when start event is emitted");

    pb.on("stop", onStop);
    pb.deactivate();
    start = true;
  }

  function onStart2() {
    assert.ok(start, "start listener must be called already");
    assert.equal(false, stop, "stop callback must not be called yet");
  }

  function finish() {
    assert.ok(pbUtils.getMode(), true,
                "private mode is active when start event is emitted");
    assert.ok(pb.isActive,
                "`isActive` is `true` when start event is emitted");
    assert.ok(pb.isPrivate(),
                "`isPrivate()` is `true` when start event is emitted");

    pb.removeListener("start", finish);
    pb.removeListener("stop", onStop);

    pb.deactivate();
    pb.once("stop", function () {
      assert.equal(pbUtils.getMode(), false);
      assert.equal(pb.isActive, false);
      assert.equal(pb.isPrivate(), false);

      done();
    });
  }

  pb.on("start", onStart);
  pb.on("start", onStart2);
  pb.activate();
};

exports.testAutomaticUnload = function(assert, done) {
  setPref(DEPRECATE_PREF, true);

  // Create another private browsing instance and unload it
  let { loader, errors } = LoaderWithHookedConsole(module);
  let pb2 = loader.require("sdk/private-browsing");
  let called = false;
  pb2.on("start", function onStart() {
    called = true;
    assert.fail("should not be called:x");
  });
  loader.unload();

  // Then switch to private mode in order to check that the previous instance
  // is correctly destroyed
  pb.once("start", function onStart() {
    timer.setTimeout(function () {
      assert.ok(!called, 
        "First private browsing instance is destroyed and inactive");
      // Must reset to normal mode, so that next test starts with it.
      deactivate(function() {
        assert.ok(errors.length, 0, "should have been 1 deprecation error");
        done();
      });
    }, 0);
  });

  pb.activate();
};

exports.testUnloadWhileActive = function(assert, done) {
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
    assert.ok(unloadHappened, "the unload event should have already occurred.");
    assert.fail("stop should not have been fired");
  });
  pb.once("stop", function() {
    assert.ok(!called, "stop was not called on unload");
    assert.ok(errors.length, 2, "should have been 2 deprecation errors");
    done();
  });

  pb.activate();
};

exports.testIgnoreWindow = function(assert, done) {
  let window = getMostRecentBrowserWindow();

  pb.once('start', function() {
    assert.ok(isWindowPrivate(window), 'window is private');
    assert.ok(!pbUtils.ignoreWindow(window), 'window is not ignored');
    pb.once('stop', done);
    pb.deactivate();
  });
  pb.activate();
};
