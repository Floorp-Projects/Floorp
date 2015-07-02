/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Hotkey } = require("sdk/hotkeys");
const { keyDown } = require("sdk/dom/events/keys");
const { Loader } = require('sdk/test/loader');
const { getMostRecentBrowserWindow } = require("sdk/window/utils");

const element = getMostRecentBrowserWindow().document.documentElement;

exports["test hotkey: function key"] = function(assert, done) {
  var showHotKey = Hotkey({
    combo: "f1",
    onPress: function() {
      assert.pass("first callback is called");
      assert.equal(this, showHotKey,
        'Context `this` in `onPress` should be the hotkey object');
      keyDown(element, "f2");
      showHotKey.destroy();
    }
  });

  var hideHotKey = Hotkey({
    combo: "f2",
    onPress: function() {
      assert.pass("second callback is called");
      hideHotKey.destroy();
      done();
    }
  });

  keyDown(element, "f1");
};

exports["test hotkey: accel alt shift"] = function(assert, done) {
  var showHotKey = Hotkey({
    combo: "accel-shift-6",
    onPress: function() {
      assert.pass("first callback is called");
      keyDown(element, "accel-alt-shift-6");
      showHotKey.destroy();
    }
  });

  var hideHotKey = Hotkey({
    combo: "accel-alt-shift-6",
    onPress: function() {
      assert.pass("second callback is called");
      hideHotKey.destroy();
      done();
    }
  });

  keyDown(element, "accel-shift-6");
};

exports["test hotkey meta & control"] = function(assert, done) {
  var showHotKey = Hotkey({
    combo: "meta-3",
    onPress: function() {
      assert.pass("first callback is called");
      keyDown(element, "alt-control-shift-b");
      showHotKey.destroy();
    }
  });

  var hideHotKey = Hotkey({
    combo: "Ctrl-Alt-Shift-B",
    onPress: function() {
      assert.pass("second callback is called");
      hideHotKey.destroy();
      done();
    }
  });

  keyDown(element, "meta-3");
};

exports["test hotkey: control-1 / meta--"] = function(assert, done) {
  var showHotKey = Hotkey({
    combo: "control-1",
    onPress: function() {
      assert.pass("first callback is called");
      keyDown(element, "meta--");
      showHotKey.destroy();
    }
  });

  var hideHotKey = Hotkey({
    combo: "meta--",
    onPress: function() {
      assert.pass("second callback is called");
      hideHotKey.destroy();
      done();
    }
  });

  keyDown(element, "control-1");
};

exports["test invalid combos"] = function(assert) {
  assert.throws(function() {
    Hotkey({
      combo: "d",
      onPress: function() {}
    });
  }, "throws if no modifier is present");
  assert.throws(function() {
    Hotkey({
      combo: "alt",
      onPress: function() {}
    });
  }, "throws if no key is present");
  assert.throws(function() {
    Hotkey({
      combo: "alt p b",
      onPress: function() {}
    });
  }, "throws if more then one key is present");
};

exports["test no exception on unmodified keypress"] = function(assert) {
  var someHotkey = Hotkey({
    combo: "control-alt-1",
    onPress: () => {}
  });
  keyDown(element, "a");
  assert.pass("No exception throw, unmodified keypress passed");
  someHotkey.destroy();
};

exports["test hotkey: automatic destroy"] = function*(assert) {
  // Hacky way to be able to create unloadable modules via makeSandboxedLoader.
  let loader = Loader(module);

  var called = false;
  var hotkey = loader.require("sdk/hotkeys").Hotkey({
    combo: "accel-shift-x",
    onPress: () => called = true
  });

  // Unload the module so that previous hotkey is automatically destroyed
  loader.unload();

  // Ensure that the hotkey is really destroyed
  keyDown(element, "accel-shift-x");

  assert.ok(!called, "Hotkey is destroyed and not called.");

  // create a new hotkey for a different set
  yield new Promise(resolve => {
    let key = Hotkey({
      combo: "accel-shift-y",
      onPress: () => {
        key.destroy();
        assert.pass("accel-shift-y was pressed.");
        resolve();
      }
    });
    keyDown(element, "accel-shift-y");
  });

  assert.ok(!called, "Hotkey is still not called, in time it would take.");

  // create a new hotkey for the same set
  yield new Promise(resolve => {
    let key = Hotkey({
      combo: "accel-shift-x",
      onPress: () => {
        key.destroy();
        assert.pass("accel-shift-x was pressed.");
        resolve();
      }
    });
    keyDown(element, "accel-shift-x");
  });

  assert.ok(!called, "Hotkey is still not called, and reusing is ok.");
};

require("sdk/test").run(exports);
