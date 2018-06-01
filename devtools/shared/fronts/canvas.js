/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  frameSnapshotSpec,
  canvasSpec,
  CANVAS_CONTEXTS,
  ANIMATION_GENERATORS,
  LOOP_GENERATORS,
  DRAW_CALLS,
  INTERESTING_CALLS,
} = require("devtools/shared/specs/canvas");
const protocol = require("devtools/shared/protocol");
const promise = require("promise");

/**
 * The corresponding Front object for the FrameSnapshotActor.
 */
const FrameSnapshotFront = protocol.FrontClassWithSpec(frameSnapshotSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this._animationFrameEndScreenshot = null;
    this._cachedScreenshots = new WeakMap();
  },

  /**
   * This implementation caches the animation frame end screenshot to optimize
   * frontend requests to `generateScreenshotFor`.
   */
  getOverview: protocol.custom(function() {
    return this._getOverview().then(data => {
      this._animationFrameEndScreenshot = data.screenshot;
      return data;
    });
  }, {
    impl: "_getOverview"
  }),

  /**
   * This implementation saves a roundtrip to the backend if the screenshot
   * was already generated and retrieved once.
   */
  generateScreenshotFor: protocol.custom(function(functionCall) {
    if (CanvasFront.ANIMATION_GENERATORS.has(functionCall.name) ||
        CanvasFront.LOOP_GENERATORS.has(functionCall.name)) {
      return promise.resolve(this._animationFrameEndScreenshot);
    }
    const cachedScreenshot = this._cachedScreenshots.get(functionCall);
    if (cachedScreenshot) {
      return cachedScreenshot;
    }
    const screenshot = this._generateScreenshotFor(functionCall);
    this._cachedScreenshots.set(functionCall, screenshot);
    return screenshot;
  }, {
    impl: "_generateScreenshotFor"
  })
});

exports.FrameSnapshotFront = FrameSnapshotFront;

/**
 * The corresponding Front object for the CanvasActor.
 */
const CanvasFront = protocol.FrontClassWithSpec(canvasSpec, {
  initialize: function(client, { canvasActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: canvasActor });
    this.manage(this);
  }
});

/**
 * Constants.
 */
CanvasFront.CANVAS_CONTEXTS = new Set(CANVAS_CONTEXTS);
CanvasFront.ANIMATION_GENERATORS = new Set(ANIMATION_GENERATORS);
CanvasFront.LOOP_GENERATORS = new Set(LOOP_GENERATORS);
CanvasFront.DRAW_CALLS = new Set(DRAW_CALLS);
CanvasFront.INTERESTING_CALLS = new Set(INTERESTING_CALLS);
CanvasFront.THUMBNAIL_SIZE = 50;
CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT = 256;
CanvasFront.INVALID_SNAPSHOT_IMAGE = {
  index: -1,
  width: 0,
  height: 0,
  pixels: []
};

exports.CanvasFront = CanvasFront;
