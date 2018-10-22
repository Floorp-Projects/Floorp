/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module defines the routines used when updating the graphics shown by a
// middleman process tab. Middleman processes have their own window/document
// which are connected to the compositor in the UI process in the usual way.
// We need to update the contents of the document to draw the raw graphics data
// provided by the child process.

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

const CC = Components.Constructor;

// Create a sandbox with the resources we need. require() doesn't work here.
const sandbox = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")());
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
  "addDebuggerToGlobal(this);",
  sandbox
);
const RecordReplayControl = sandbox.RecordReplayControl;

// State for dragging the overlay.
let dragx, dragy;

// Windows in the middleman process are initially set up as about:blank pages.
// This method fills them in with a canvas filling the tab, and an overlay that
// can be displayed over that canvas.
function setupContents(window) {
  // The middlemanOverlay element is shown when the active child is replaying.
  const overlay = window.middlemanOverlay = window.document.createElement("div");
  overlay.style.position = "absolute";
  overlay.style.border = "medium solid #000000";
  overlay.style.backgroundColor = "#BBCCCC";

  // The middlemanPosition element is contained in the overlay and shows the
  // current position in the recording.
  const position = window.middlemanPosition = window.document.createElement("div");
  position.innerText = "";
  position.style.textAlign = "center";
  position.style.padding = "5px 5px 0px 5px";
  overlay.appendChild(position);

  // The middlemanProgressBar element is contained in the overlay and shows any
  // progress made on the current operation.
  const progressBar =
    window.middlemanProgressBar = window.document.createElement("canvas");
  progressBar.width = 100;
  progressBar.height = 5;
  progressBar.getContext("2d").fillStyle = "white";
  progressBar.getContext("2d").fillRect(0, 0, 100, 5);
  progressBar.style.padding = "5px 5px 5px 5px";

  overlay.appendChild(progressBar);
  window.document.body.prepend(overlay);

  overlay.onmousedown = window.middlemanMouseDown = function(e) {
    e.preventDefault();
    dragx = e.clientX;
    dragy = e.clientY;
    window.document.onmouseup = window.middlemanMouseUp;
    window.document.onmousemove = window.middlemanMouseMove;
  };

  window.middlemanMouseMove = function(e) {
    // Compute the new position of the overlay per the current drag operation.
    // Don't allow the overlay to be dragged outside the window.
    e.preventDefault();
    const canvas = window.middlemanCanvas;
    let diffx = e.clientX - dragx;
    let diffy = e.clientY - dragy;
    const newTop = () => overlay.offsetTop + diffy;
    if (newTop() < 0) {
      diffy -= newTop();
    }
    const maxTop = canvas.height / window.devicePixelRatio;
    if (newTop() + overlay.offsetHeight >= maxTop) {
      diffy -= newTop() + overlay.offsetHeight - maxTop;
    }
    overlay.style.top = newTop() + "px";
    const newLeft = () => overlay.offsetLeft + diffx;
    if (newLeft() < 0) {
      diffx -= newLeft();
    }
    const maxLeft = canvas.width / window.devicePixelRatio;
    if (newLeft() + overlay.offsetWidth >= maxLeft) {
      diffx -= newLeft() + overlay.offsetWidth - maxLeft;
    }
    overlay.style.left = (overlay.offsetLeft + diffx) + "px";
    dragx += diffx;
    dragy += diffy;
  };

  window.middlemanMouseUp = function(e) {
    window.document.onmouseup = null;
    window.document.onmousemove = null;
  };

  // The middlemanCanvas element fills the tab's contents.
  const canvas = window.middlemanCanvas = window.document.createElement("canvas");
  canvas.style.position = "absolute";
  window.document.body.style.margin = "0px";
  window.document.body.prepend(canvas);
}

function getOverlay(window) {
  if (!window.middlemanOverlay) {
    setupContents(window);
  }
  return window.middlemanOverlay;
}

function getCanvas(window) {
  if (!window.middlemanCanvas) {
    setupContents(window);
  }
  return window.middlemanCanvas;
}

function updateWindowCanvas(window, buffer, width, height, hadFailure) {
  // Make sure the window has a canvas filling the screen.
  const canvas = getCanvas(window);

  canvas.width = width;
  canvas.height = height;

  // If there is a scale for this window, then the graphics will already have
  // been scaled in the child process. To avoid scaling the graphics twice,
  // transform the canvas to undo the scaling.
  const scale = window.devicePixelRatio;
  if (scale != 1) {
    canvas.style.transform =
      `scale(${ 1 / scale }) translate(-${ width / scale }px, -${ height / scale }px)`;
  }

  const cx = canvas.getContext("2d");

  const graphicsData = new Uint8Array(buffer);
  const imageData = cx.getImageData(0, 0, width, height);
  imageData.data.set(graphicsData);
  cx.putImageData(imageData, 0, 0);

  // Indicate to the user when repainting failed and we are showing old painted
  // graphics instead of the most up-to-date graphics.
  if (hadFailure) {
    cx.fillStyle = "red";
    cx.font = "48px serif";
    cx.fillText("PAINT FAILURE", 10, 50);
  }

  // Make recording/replaying tabs easier to differentiate from other tabs.
  window.document.title = "RECORD/REPLAY";

  updateWindowOverlay(window);
}

// Entry point for when we have some new graphics data from the child process
// to draw.
// eslint-disable-next-line no-unused-vars
function UpdateCanvas(buffer, width, height, hadFailure) {
  try {
    // Paint to all windows we can find. Hopefully there is only one.
    for (const window of Services.ww.getWindowEnumerator()) {
      updateWindowCanvas(window, buffer, width, height, hadFailure);
    }
  } catch (e) {
    dump("Middleman Graphics UpdateCanvas Exception: " + e + "\n");
  }
}

function updateWindowOverlay(window) {
  const overlay = getOverlay(window);

  const position = RecordReplayControl.recordingPosition();
  if (position === undefined) {
    overlay.style.visibility = "hidden";
  } else {
    overlay.style.visibility = "visible";
    window.middlemanPosition.innerText = (Math.round(position * 10000) / 100) + "%";
  }
}

// Entry point for when we need to update the overlay's contents or visibility.
// eslint-disable-next-line no-unused-vars
function UpdateOverlay() {
  try {
    // Paint to all windows we can find. Hopefully there is only one.
    for (const window of Services.ww.getWindowEnumerator()) {
      updateWindowOverlay(window);
    }
  } catch (e) {
    dump("Middleman Graphics UpdateOverlay Exception: " + e + "\n");
  }
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "UpdateCanvas",
  "UpdateOverlay",
];
