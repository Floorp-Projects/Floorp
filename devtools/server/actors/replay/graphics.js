/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module defines the routines used when updating the graphics shown by a
// middleman process tab. Middleman processes have their own window/document
// which are connected to the compositor in the UI process in the usual way.
// We need to update the contents of the document to draw the raw graphics data
// provided by the child process.

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const CC = Components.Constructor;

// Create a sandbox with the resources we need. require() doesn't work here.
const sandbox = Cu.Sandbox(
  CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")()
);
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
    "addDebuggerToGlobal(this);",
  sandbox
);

// Windows in the middleman process are initially set up as about:blank pages.
// This method fills them in with a canvas filling the tab.
function setupContents(window) {
  // The middlemanCanvas element fills the tab's contents.
  const canvas = (window.middlemanCanvas = window.document.createElement(
    "canvas"
  ));
  canvas.style.position = "absolute";
  window.document.body.style.margin = "0px";
  window.document.body.prepend(canvas);
}

function getCanvas(window) {
  if (!window.middlemanCanvas) {
    setupContents(window);
  }
  return window.middlemanCanvas;
}

function updateWindowCanvas(window, buffer, width, height) {
  // Make sure the window has a canvas filling the screen.
  const canvas = getCanvas(window);

  canvas.width = width;
  canvas.height = height;

  // If there is a scale for this window, then the graphics will already have
  // been scaled in the child process. To avoid scaling the graphics twice,
  // transform the canvas to undo the scaling.
  const scale = window.devicePixelRatio;
  if (scale != 1) {
    canvas.style.transform = `scale(${1 / scale}) translate(-${width /
      scale}px, -${height / scale}px)`;
  }

  const cx = canvas.getContext("2d");

  const graphicsData = new Uint8Array(buffer);
  const imageData = cx.getImageData(0, 0, width, height);
  imageData.data.set(graphicsData);
  cx.putImageData(imageData, 0, 0);
}

function clearWindowCanvas(window) {
  const canvas = getCanvas(window);

  const cx = canvas.getContext("2d");
  cx.clearRect(0, 0, canvas.width, canvas.height);
}

// Entry point for when we have some new graphics data from the child process
// to draw.
// eslint-disable-next-line no-unused-vars
function UpdateCanvas(buffer, width, height) {
  try {
    // Paint to all windows we can find. Hopefully there is only one.
    for (const window of Services.ww.getWindowEnumerator()) {
      updateWindowCanvas(window, buffer, width, height);
    }
  } catch (e) {
    dump(`Middleman Graphics UpdateCanvas Exception: ${e}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
function ClearCanvas() {
  try {
    for (const window of Services.ww.getWindowEnumerator()) {
      clearWindowCanvas(window);
    }
  } catch (e) {
    dump(`Middleman Graphics ClearCanvas Exception: ${e}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["UpdateCanvas", "ClearCanvas"];
