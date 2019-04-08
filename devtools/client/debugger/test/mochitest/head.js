/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * The Mochitest API documentation
 * @module mochitest
 */

/**
 * The mochitest API to wait for certain events.
 * @module mochitest/waits
 * @parent mochitest
 */

/**
 * The mochitest API predefined asserts.
 * @module mochitest/asserts
 * @parent mochitest
 */

/**
 * The mochitest API for interacting with the debugger.
 * @module mochitest/actions
 * @parent mochitest
 */

/**
 * Helper methods for the mochitest API.
 * @module mochitest/helpers
 * @parent mochitest
 */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers.js",
  this
);

const EXAMPLE_URL =
  "http://example.com/browser/devtools/client/debugger/test/mochitest/examples/";


// NOTE: still experimental, the screenshots might not be exactly correct
async function takeScreenshot(dbg) {
  let canvas = dbg.win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "html:canvas"
  );
  let context = canvas.getContext("2d");
  canvas.width = dbg.win.innerWidth;
  canvas.height = dbg.win.innerHeight;
  context.drawWindow(dbg.win, 0, 0, canvas.width, canvas.height, "white");
  await waitForTime(1000);
  dump(`[SCREENSHOT] ${canvas.toDataURL()}\n`);
}
