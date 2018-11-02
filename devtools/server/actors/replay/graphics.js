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

function el(window, tag, attrs, ...children) {
  const node = window.document.createElement(tag);
  for (const attr in attrs) {
    node[attr] = attrs[attr];
  }

  if (attrs) {
    for (const child of children) {
      node.appendChild(child);
    }
  }

  return node;
}

// Windows in the middleman process are initially set up as about:blank pages.
// This method fills them in with a canvas filling the tab, and an overlay that
// can be displayed over that canvas.
function setupContents(window) {
  // The overlay shows the recording timeline
  window.document.body.appendChild(createOverlay(window));

  window.document.querySelector(".play-button")
    .onclick = () => RecordReplayControl.resume(true);
  window.document.querySelector(".rewind-button")
    .onclick = () => RecordReplayControl.resume(false);
  window.document.querySelector(".pause-button")
    .onclick = () => RecordReplayControl.pause();

  // The middlemanCanvas element fills the tab's contents.
  const canvas = window.middlemanCanvas = window.document.createElement("canvas");
  canvas.style.position = "absolute";
  window.document.body.style.margin = "0px";
  window.document.body.prepend(canvas);
}

function createOverlay(window) {
  const div = (attrs, ...children) => el(window, "div", attrs, ...children);

  const overlayStyles = `
    :root {
      --pause-image: url('data:image/svg+xml;utf8,<svg width="16" height="16" viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><g fill-rule="evenodd"><path d="M5 12.503l.052-9a.5.5 0 0 0-1-.006l-.052 9a.5.5 0 0 0 1 .006zM12 12.497l-.05-9A.488.488 0 0 0 11.474 3a.488.488 0 0 0-.473.503l.05 9a.488.488 0 0 0 .477.497.488.488 0 0 0 .473-.503z"/></g></svg>');
      --play-image: url('data:image/svg+xml;utf8,<svg width="16" height="16" xmlns="http://www.w3.org/2000/svg"><g fill-rule="evenodd"><path d="M4 12.5l8-5-8-5v10zm-1 0v-10a1 1 0 0 1 1.53-.848l8 5a1 1 0 0 1 0 1.696l-8 5A1 1 0 0 1 3 12.5z" /></g></svg>');
    }

    #overlay {
      position: absolute;
      background: #F9F9FA;
      width: 100%;
      height: 29px;
      left: 0px;
      bottom: 0px;
      border-top: 1px solid #DCE1E4;
    }

    .overlay-container {
      display: flex;
    }

    .progressBar {
      position: relative;
      width: 100%;
      height: 6px;
      background: #DCDCDC;
      margin: 11px 10px 11px 0;
    }

    .progress {
      position: absolute;
      width: 0;
      height: 100%;
      background: #B7B6B6;
    }

    .commands {
      display: flex;
    }

    .command-button {
      display: flex;
    }

    .command-button:hover {
      background: #efefef;
    }

    .btn {
      width: 15px;
      height: 19px;
      background: #6A6A6A;
      mask-size: 15px 19px;
      align-self: center;
    }

    .play-button {
      mask-image: var(--play-image);
      margin-right: 5px;
      margin-left: 2px;
      display: none;
    }
    .rewind-button {
      mask-image: var(--play-image);
      transform: scaleX(-1);
      margin-left: 5px;
      margin-right: 2px;
      display: none;
    }
    .pause-button {
      mask-image: var(--pause-image);
      margin-left: 5px;
      margin-right: 10px;      
    }

    .paused .pause-button {
      display: none;
    }

    .paused .play-button {
      display: block;
    }
    .paused .rewind-button {
      display: block;
    }
  `;

  return div(
    { id: "overlay", className: "paused" },
    el(window, "style", { innerHTML: overlayStyles }),
    div(
      { className: "overlay-container " },
      div(
        { className: "commands" },
        div(
          { className: "command-button" },
          div({ className: "rewind-button btn" })
        ),
        div(
          { className: "command-button" },
          div({ className: "play-button btn" })
        ),
        div(
          { className: "command-button" },
          div({ className: "pause-button btn" })
        )
      ),
      div({ className: "progressBar" }, div({ className: "progress" }))
    )
  );
}

function getOverlay(window) {
  if (!window.document.querySelector("#overlay")) {
    setupContents(window);
  }
  return window.document.querySelector("#overlay");
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
  const position = RecordReplayControl.recordingPosition();
  overlay.className = position ? "paused" : "";
  overlay.querySelector(".progress")
    .style.width = (Math.round((position || 1) * 10000) / 100) + "%";
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
