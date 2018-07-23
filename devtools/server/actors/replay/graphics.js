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

function updateWindow(window, buffer, width, height) {
  // Make sure the window has a canvas filling the screen.
  let canvas = window.middlemanCanvas;
  if (!canvas) {
    canvas = window.document.createElement("canvas");
    window.document.body.style.margin = "0px";
    window.document.body.insertBefore(canvas, window.document.body.firstChild);
    window.middlemanCanvas = canvas;
  }

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

  const graphicsData = new Uint8Array(buffer);
  const imageData = canvas.getContext("2d").getImageData(0, 0, width, height);
  imageData.data.set(graphicsData);
  canvas.getContext("2d").putImageData(imageData, 0, 0);

  // Make recording/replaying tabs easier to differentiate from other tabs.
  window.document.title = "RECORD/REPLAY";
}

// Entry point for when we have some new graphics data from the child process
// to draw.
// eslint-disable-next-line no-unused-vars
function Update(buffer, width, height) {
  try {
    // Paint to all windows we can find. Hopefully there is only one.
    const windowEnumerator = Services.ww.getWindowEnumerator();
    while (windowEnumerator.hasMoreElements()) {
      const window = windowEnumerator.getNext().QueryInterface(Ci.nsIDOMWindow);
      updateWindow(window, buffer, width, height);
    }
  } catch (e) {
    dump("Middleman Graphics Update Exception: " + e + "\n");
  }
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "Update",
];
