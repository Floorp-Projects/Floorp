/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../shared/test/telemetry-test-helpers.js */

"use strict";

const { require, loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/telemetry-test-helpers.js", this);

/* exported loader, either, click, dblclick, mousedown, rightMousedown, key */
// All tests are asynchronous.
waitForExplicitFinish();

// Performance tests are much heavier because of their reliance on the
// profiler module, memory measurements, frequent canvas operations etc. Many of
// of them take longer than 30 seconds to finish on try server VMs, even though
// they superficially do very little.
requestLongerTimeout(3);

// Same as `is`, but takes in two possible values.
const either = (value, a, b, message) => {
  if (value == a) {
    is(value, a, message);
  } else if (value == b) {
    is(value, b, message);
  } else {
    ok(false, message);
  }
};

// Shortcut for simulating a click on an element.
const click = (node, win = window) => {
  EventUtils.sendMouseEvent({ type: "click" }, node, win);
};

// Shortcut for simulating a double click on an element.
const dblclick = (node, win = window) => {
  EventUtils.sendMouseEvent({ type: "dblclick" }, node, win);
};

// Shortcut for simulating a mousedown on an element.
const mousedown = (node, win = window) => {
  EventUtils.sendMouseEvent({ type: "mousedown" }, node, win);
};

// Shortcut for simulating a mousedown using the right mouse button on an element.
const rightMousedown = (node, win = window) => {
  EventUtils.sendMouseEvent({ type: "mousedown", button: 2 }, node, win);
};

// Shortcut for firing a key event, like "VK_UP", "VK_DOWN", etc.
const key = (id, win = window) => {
  EventUtils.synthesizeKey(id, {}, win);
};

// Don't pollute global scope.
(() => {
  const PrefUtils = require("devtools/client/performance/test/helpers/prefs");

  // Make sure all the prefs are reverted to their defaults once tests finish.
  const stopObservingPrefs = PrefUtils.whenUnknownPrefChanged("devtools.performance",
    pref => {
      ok(false, `Unknown pref changed: ${pref}. Please add it to test/helpers/prefs.js ` +
        "to make sure it's reverted to its default value when the tests finishes, " +
        "and avoid interfering with future tests.\n");
    });

  // By default, enable memory flame graphs for tests for now.
  // TODO: remove when we have flame charts via bug 1148663.
  Services.prefs.setBoolPref(PrefUtils.UI_ENABLE_MEMORY_FLAME_CHART, true);

  registerCleanupFunction(() => {
    info("finish() was called, cleaning up...");

    PrefUtils.rollbackPrefsToDefault();
    stopObservingPrefs();

    // Manually stop the profiler module at the end of all tests, to hopefully
    // avoid at least some leaks on OSX. Theoretically the module should never
    // be active at this point. We shouldn't have to do this, but rather
    // find and fix the leak in the module itself. Bug 1257439.
    Services.profiler.StopProfiler();

    // Forces GC, CC and shrinking GC to get rid of disconnected docshells
    // and windows.
    Cu.forceGC();
    Cu.forceCC();
    Cu.forceShrinkingGC();
  });
})();
