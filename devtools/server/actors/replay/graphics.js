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

// Windows in the middleman process are initially set up as about:blank pages.
// This method fills them in with a canvas filling the tab, and an overlay that
// can be displayed over that canvas.
function setupContents(window) {
  // The middlemanOverlay element is shown when the active child is replaying.
  const overlay = (window.middlemanOverlay = window.document.createElement("div"));
  overlay.style.position = "absolute";
  overlay.style.visibility = "hidden";
  overlay.style.backgroundColor = "#F9F9FA";
  overlay.style.width = "100%";
  overlay.style.height = "29px";
  overlay.style.left = "0px";
  overlay.style.bottom = "0px";
  overlay.style["border-top"] = "1px solid #DCE1E4";

  // The middlemanProgressBar element is contained in the overlay and shows any
  // progress made on the current operation.
  const progressBar = window.document.createElement("div");
  progressBar.style.position = "relative";
  progressBar.style.width = "calc(100% - 20px)";
  progressBar.style.height = "6px";
  progressBar.style.background = "#DCDCDC";
  progressBar.style.margin = "11px 10px";
  overlay.appendChild(progressBar);

  const progress = (window.middlemanProgress = window.document.createElement("div"));
  progress.style.position = "absolute";
  progress.style.width = "0";
  progress.style.height = "100%";
  progress.style.background = "#B7B6B6";

  progressBar.appendChild(progress);

  window.document.body.prepend(overlay);

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
  if (!Services.prefs.getBoolPref("devtools.recordreplay.timeline.enabled")) {
    return;
  }

  const overlay = getOverlay(window);
  const position = RecordReplayControl.recordingPosition() || 1;
  if (position === undefined) {
    overlay.style.visibility = "hidden";
  } else {
    overlay.style.visibility = "visible";
    window.middlemanProgress.style.width = (Math.round(position * 10000) / 100) + "%";
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
