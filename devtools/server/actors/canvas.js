/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const events = require("sdk/event/core");
const promise = require("promise");
const protocol = require("devtools/shared/protocol");
const {CallWatcherActor} = require("devtools/server/actors/call-watcher");
const {CallWatcherFront} = require("devtools/shared/fronts/call-watcher");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const {WebGLPrimitiveCounter} = require("devtools/server/primitive");

const {on, once, off, emit} = events;
const {method, custom, Arg, Option, RetVal} = protocol;

const CANVAS_CONTEXTS = [
  "CanvasRenderingContext2D",
  "WebGLRenderingContext"
];

const ANIMATION_GENERATORS = [
  "requestAnimationFrame"
];

const LOOP_GENERATORS = [
  "setTimeout"
];

const DRAW_CALLS = [
  // 2D canvas
  "fill",
  "stroke",
  "clearRect",
  "fillRect",
  "strokeRect",
  "fillText",
  "strokeText",
  "drawImage",

  // WebGL
  "clear",
  "drawArrays",
  "drawElements",
  "finish",
  "flush"
];

const INTERESTING_CALLS = [
  // 2D canvas
  "save",
  "restore",

  // WebGL
  "useProgram"
];

/**
 * Type representing an ArrayBufferView, serialized fast(er).
 *
 * Don't create a new array buffer view from the parsed array on the frontend.
 * Consumers may copy the data into an existing buffer, or create a new one if
 * necesasry. For example, this avoids the need for a redundant copy when
 * populating ImageData objects, at the expense of transferring char views
 * of a pixel buffer over the protocol instead of a packed int view.
 *
 * XXX: It would be nice if on local connections (only), we could just *give*
 * the buffer directly to the front, instead of going through all this
 * serialization redundancy.
 */
protocol.types.addType("array-buffer-view", {
  write: (v) => "[" + Array.join(v, ",") + "]",
  read: (v) => JSON.parse(v)
});

/**
 * Type describing a thumbnail or screenshot in a recorded animation frame.
 */
protocol.types.addDictType("snapshot-image", {
  index: "number",
  width: "number",
  height: "number",
  scaling: "number",
  flipped: "boolean",
  pixels: "array-buffer-view"
});

/**
 * Type describing an overview of a recorded animation frame.
 */
protocol.types.addDictType("snapshot-overview", {
  calls: "array:function-call",
  thumbnails: "array:snapshot-image",
  screenshot: "snapshot-image"
});

/**
 * This actor represents a recorded animation frame snapshot, along with
 * all the corresponding canvas' context methods invoked in that frame,
 * thumbnails for each draw call and a screenshot of the end result.
 */
var FrameSnapshotActor = protocol.ActorClass({
  typeName: "frame-snapshot",

  /**
   * Creates the frame snapshot call actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param HTMLCanvasElement canvas
   *        A reference to the content canvas.
   * @param array calls
   *        An array of "function-call" actor instances.
   * @param object screenshot
   *        A single "snapshot-image" type instance.
   */
  initialize: function (conn, { canvas, calls, screenshot, primitive }) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._contentCanvas = canvas;
    this._functionCalls = calls;
    this._animationFrameEndScreenshot = screenshot;
    this._primitive = primitive;
  },

  /**
   * Gets as much data about this snapshot without computing anything costly.
   */
  getOverview: method(function () {
    return {
      calls: this._functionCalls,
      thumbnails: this._functionCalls.map(e => e._thumbnail).filter(e => !!e),
      screenshot: this._animationFrameEndScreenshot,
      primitive: {
        tris: this._primitive.tris,
        vertices: this._primitive.vertices,
        points: this._primitive.points,
        lines: this._primitive.lines
      }
    };
  }, {
    response: { overview: RetVal("snapshot-overview") }
  }),

  /**
   * Gets a screenshot of the canvas's contents after the specified
   * function was called.
   */
  generateScreenshotFor: method(function (functionCall) {
    let caller = functionCall.details.caller;
    let global = functionCall.details.global;

    let canvas = this._contentCanvas;
    let calls = this._functionCalls;
    let index = calls.indexOf(functionCall);

    // To get a screenshot, replay all the steps necessary to render the frame,
    // by invoking the context calls up to and including the specified one.
    // This will be done in a custom framebuffer in case of a WebGL context.
    let replayData = ContextUtils.replayAnimationFrame({
      contextType: global,
      canvas: canvas,
      calls: calls,
      first: 0,
      last: index
    });

    let { replayContext, replayContextScaling, lastDrawCallIndex, doCleanup } = replayData;
    let [left, top, width, height] = replayData.replayViewport;
    let screenshot;

    // Depending on the canvas' context, generating a screenshot is done
    // in different ways.
    if (global == "WebGLRenderingContext") {
      screenshot = ContextUtils.getPixelsForWebGL(replayContext, left, top, width, height);
      screenshot.flipped = true;
    } else if (global == "CanvasRenderingContext2D") {
      screenshot = ContextUtils.getPixelsFor2D(replayContext, left, top, width, height);
      screenshot.flipped = false;
    }

    // In case of the WebGL context, we also need to reset the framebuffer
    // binding to the original value, after generating the screenshot.
    doCleanup();

    screenshot.scaling = replayContextScaling;
    screenshot.index = lastDrawCallIndex;
    return screenshot;
  }, {
    request: { call: Arg(0, "function-call") },
    response: { screenshot: RetVal("snapshot-image") }
  })
});

/**
 * The corresponding Front object for the FrameSnapshotActor.
 */
var FrameSnapshotFront = protocol.FrontClass(FrameSnapshotActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this._animationFrameEndScreenshot = null;
    this._cachedScreenshots = new WeakMap();
  },

  /**
   * This implementation caches the animation frame end screenshot to optimize
   * frontend requests to `generateScreenshotFor`.
   */
  getOverview: custom(function () {
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
  generateScreenshotFor: custom(function (functionCall) {
    if (CanvasFront.ANIMATION_GENERATORS.has(functionCall.name) ||
        CanvasFront.LOOP_GENERATORS.has(functionCall.name)) {
      return promise.resolve(this._animationFrameEndScreenshot);
    }
    let cachedScreenshot = this._cachedScreenshots.get(functionCall);
    if (cachedScreenshot) {
      return cachedScreenshot;
    }
    let screenshot = this._generateScreenshotFor(functionCall);
    this._cachedScreenshots.set(functionCall, screenshot);
    return screenshot;
  }, {
    impl: "_generateScreenshotFor"
  })
});

/**
 * This Canvas Actor handles simple instrumentation of all the methods
 * of a 2D or WebGL context, to provide information regarding all the calls
 * made when drawing frame inside an animation loop.
 */
var CanvasActor = exports.CanvasActor = protocol.ActorClass({
  // Reset for each recording, boolean indicating whether or not
  // any draw calls were called for a recording.
  _animationContainsDrawCall: false,

  typeName: "canvas",
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._webGLPrimitiveCounter = new WebGLPrimitiveCounter(tabActor);
    this._onContentFunctionCall = this._onContentFunctionCall.bind(this);
  },
  destroy: function (conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this._webGLPrimitiveCounter.destroy();
    this.finalize();
  },

  /**
   * Starts listening for function calls.
   */
  setup: method(function ({ reload }) {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    this._callWatcher = new CallWatcherActor(this.conn, this.tabActor);
    this._callWatcher.onCall = this._onContentFunctionCall;
    this._callWatcher.setup({
      tracedGlobals: CANVAS_CONTEXTS,
      tracedFunctions: [...ANIMATION_GENERATORS, ...LOOP_GENERATORS],
      performReload: reload,
      storeCalls: true
    });
  }, {
    request: { reload: Option(0, "boolean") },
    oneway: true
  }),

  /**
   * Stops listening for function calls.
   */
  finalize: method(function () {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    this._callWatcher.finalize();
    this._callWatcher = null;
  }, {
    oneway: true
  }),

  /**
   * Returns whether this actor has been set up.
   */
  isInitialized: method(function () {
    return !!this._initialized;
  }, {
    response: { initialized: RetVal("boolean") }
  }),

  /**
   * Returns whether or not the CanvasActor is recording an animation.
   * Used in tests.
   */
  isRecording: method(function () {
    return !!this._callWatcher.isRecording();
  }, {
    response: { recording: RetVal("boolean") }
  }),

  /**
   * Records a snapshot of all the calls made during the next animation frame.
   * The animation should be implemented via the de-facto requestAnimationFrame
   * utility, or inside recursive `setTimeout`s. `setInterval` at this time are not supported.
   */
  recordAnimationFrame: method(function () {
    if (this._callWatcher.isRecording()) {
      return this._currentAnimationFrameSnapshot.promise;
    }

    this._recordingContainsDrawCall = false;
    this._callWatcher.eraseRecording();
    this._callWatcher.initTimestampEpoch();
    this._webGLPrimitiveCounter.resetCounts();
    this._callWatcher.resumeRecording();

    let deferred = this._currentAnimationFrameSnapshot = promise.defer();
    return deferred.promise;
  }, {
    response: { snapshot: RetVal("nullable:frame-snapshot") }
  }),

  /**
   * Cease attempts to record an animation frame.
   */
  stopRecordingAnimationFrame: method(function () {
    if (!this._callWatcher.isRecording()) {
      return;
    }
    this._animationStarted = false;
    this._callWatcher.pauseRecording();
    this._callWatcher.eraseRecording();
    this._currentAnimationFrameSnapshot.resolve(null);
    this._currentAnimationFrameSnapshot = null;
  }, {
    oneway: true
  }),

  /**
   * Invoked whenever an instrumented function is called, be it on a
   * 2d or WebGL context, or an animation generator like requestAnimationFrame.
   */
  _onContentFunctionCall: function (functionCall) {
    let { window, name, args } = functionCall.details;

    // The function call arguments are required to replay animation frames,
    // in order to generate screenshots. However, simply storing references to
    // every kind of object is a bad idea, since their properties may change.
    // Consider transformation matrices for example, which are typically
    // Float32Arrays whose values can easily change across context calls.
    // They need to be cloned.
    inplaceShallowCloneArrays(args, window);

    // Handle animations generated using requestAnimationFrame
    if (CanvasFront.ANIMATION_GENERATORS.has(name)) {
      this._handleAnimationFrame(functionCall);
      return;
    }
    // Handle animations generated using setTimeout. While using
    // those timers is considered extremely poor practice, they're still widely
    // used on the web, especially for old demos; it's nice to support them as well.
    if (CanvasFront.LOOP_GENERATORS.has(name)) {
      this._handleAnimationFrame(functionCall);
      return;
    }
    if (CanvasFront.DRAW_CALLS.has(name) && this._animationStarted) {
      this._handleDrawCall(functionCall);
      this._webGLPrimitiveCounter.handleDrawPrimitive(functionCall);
      return;
    }
  },

  /**
   * Handle animations generated using requestAnimationFrame.
   */
  _handleAnimationFrame: function (functionCall) {
    if (!this._animationStarted) {
      this._handleAnimationFrameBegin();
    }
    // Check to see if draw calls occurred yet, as it could be future frames,
    // like in the scenario where requestAnimationFrame is called to trigger an animation,
    // and rAF is at the beginning of the animate loop.
    else if (this._animationContainsDrawCall) {
      this._handleAnimationFrameEnd(functionCall);
    }
  },

  /**
   * Called whenever an animation frame rendering begins.
   */
  _handleAnimationFrameBegin: function () {
    this._callWatcher.eraseRecording();
    this._animationStarted = true;
  },

  /**
   * Called whenever an animation frame rendering ends.
   */
  _handleAnimationFrameEnd: function () {
    // Get a hold of all the function calls made during this animation frame.
    // Since only one snapshot can be recorded at a time, erase all the
    // previously recorded calls.
    let functionCalls = this._callWatcher.pauseRecording();
    this._callWatcher.eraseRecording();
    this._animationContainsDrawCall = false;

    // Since the animation frame finished, get a hold of the (already retrieved)
    // canvas pixels to conveniently create a screenshot of the final rendering.
    let index = this._lastDrawCallIndex;
    let width = this._lastContentCanvasWidth;
    let height = this._lastContentCanvasHeight;
    let flipped = !!this._lastThumbnailFlipped; // undefined -> false
    let pixels = ContextUtils.getPixelStorage()["8bit"];
    let primitiveResult = this._webGLPrimitiveCounter.getCounts();
    let animationFrameEndScreenshot = {
      index: index,
      width: width,
      height: height,
      scaling: 1,
      flipped: flipped,
      pixels: pixels.subarray(0, width * height * 4)
    };

    // Wrap the function calls and screenshot in a FrameSnapshotActor instance,
    // which will resolve the promise returned by `recordAnimationFrame`.
    let frameSnapshot = new FrameSnapshotActor(this.conn, {
      canvas: this._lastDrawCallCanvas,
      calls: functionCalls,
      screenshot: animationFrameEndScreenshot,
      primitive: {
        tris: primitiveResult.tris,
        vertices: primitiveResult.vertices,
        points: primitiveResult.points,
        lines: primitiveResult.lines
      }
    });

    this._currentAnimationFrameSnapshot.resolve(frameSnapshot);
    this._currentAnimationFrameSnapshot = null;
    this._animationStarted = false;
  },

  /**
   * Invoked whenever a draw call is detected in the animation frame which is
   * currently being recorded.
   */
  _handleDrawCall: function (functionCall) {
    let functionCalls = this._callWatcher.pauseRecording();
    let caller = functionCall.details.caller;
    let global = functionCall.details.global;

    let contentCanvas = this._lastDrawCallCanvas = caller.canvas;
    let index = this._lastDrawCallIndex = functionCalls.indexOf(functionCall);
    let w = this._lastContentCanvasWidth = contentCanvas.width;
    let h = this._lastContentCanvasHeight = contentCanvas.height;

    // To keep things fast, generate images of small and fixed dimensions.
    let dimensions = CanvasFront.THUMBNAIL_SIZE;
    let thumbnail;

    this._animationContainsDrawCall = true;

    // Create a thumbnail on every draw call on the canvas context, to augment
    // the respective function call actor with this additional data.
    if (global == "WebGLRenderingContext") {
      // Check if drawing to a custom framebuffer (when rendering to texture).
      // Don't create a thumbnail in this particular case.
      let framebufferBinding = caller.getParameter(caller.FRAMEBUFFER_BINDING);
      if (framebufferBinding == null) {
        thumbnail = ContextUtils.getPixelsForWebGL(caller, 0, 0, w, h, dimensions);
        thumbnail.flipped = this._lastThumbnailFlipped = true;
        thumbnail.index = index;
      }
    } else if (global == "CanvasRenderingContext2D") {
      thumbnail = ContextUtils.getPixelsFor2D(caller, 0, 0, w, h, dimensions);
      thumbnail.flipped = this._lastThumbnailFlipped = false;
      thumbnail.index = index;
    }

    functionCall._thumbnail = thumbnail;
    this._callWatcher.resumeRecording();
  }
});

/**
 * A collection of methods for manipulating canvas contexts.
 */
var ContextUtils = {
  /**
   * WebGL contexts are sensitive to how they're queried. Use this function
   * to make sure the right context is always retrieved, if available.
   *
   * @param HTMLCanvasElement canvas
   *        The canvas element for which to get a WebGL context.
   * @param WebGLRenderingContext gl
   *        The queried WebGL context, or null if unavailable.
   */
  getWebGLContext: function (canvas) {
    return canvas.getContext("webgl") ||
           canvas.getContext("experimental-webgl");
  },

  /**
   * Gets a hold of the rendered pixels in the most efficient way possible for
   * a canvas with a WebGL context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context to get a screenshot from.
   * @param number srcX [optional]
   *        The first left pixel that is read from the framebuffer.
   * @param number srcY [optional]
   *        The first top pixel that is read from the framebuffer.
   * @param number srcWidth [optional]
   *        The number of pixels to read on the X axis.
   * @param number srcHeight [optional]
   *        The number of pixels to read on the Y axis.
   * @param number dstHeight [optional]
   *        The desired generated screenshot height.
   * @return object
   *         An objet containing the screenshot's width, height and pixel data,
   *         represented as an 8-bit array buffer of r, g, b, a values.
   */
  getPixelsForWebGL: function (gl,
    srcX = 0, srcY = 0,
    srcWidth = gl.canvas.width,
    srcHeight = gl.canvas.height,
    dstHeight = srcHeight)
  {
    let contentPixels = ContextUtils.getPixelStorage(srcWidth, srcHeight);
    let { "8bit": charView, "32bit": intView } = contentPixels;
    gl.readPixels(srcX, srcY, srcWidth, srcHeight, gl.RGBA, gl.UNSIGNED_BYTE, charView);
    return this.resizePixels(intView, srcWidth, srcHeight, dstHeight);
  },

  /**
   * Gets a hold of the rendered pixels in the most efficient way possible for
   * a canvas with a 2D context.
   *
   * @param CanvasRenderingContext2D ctx
   *        The 2D context to get a screenshot from.
   * @param number srcX [optional]
   *        The first left pixel that is read from the canvas.
   * @param number srcY [optional]
   *        The first top pixel that is read from the canvas.
   * @param number srcWidth [optional]
   *        The number of pixels to read on the X axis.
   * @param number srcHeight [optional]
   *        The number of pixels to read on the Y axis.
   * @param number dstHeight [optional]
   *        The desired generated screenshot height.
   * @return object
   *         An objet containing the screenshot's width, height and pixel data,
   *         represented as an 8-bit array buffer of r, g, b, a values.
   */
  getPixelsFor2D: function (ctx,
    srcX = 0, srcY = 0,
    srcWidth = ctx.canvas.width,
    srcHeight = ctx.canvas.height,
    dstHeight = srcHeight)
  {
    let { data } = ctx.getImageData(srcX, srcY, srcWidth, srcHeight);
    let { "32bit": intView } = ContextUtils.usePixelStorage(data.buffer);
    return this.resizePixels(intView, srcWidth, srcHeight, dstHeight);
  },

  /**
   * Resizes the provided pixels to fit inside a rectangle with the specified
   * height and the same aspect ratio as the source.
   *
   * @param Uint32Array srcPixels
   *        The source pixel data, assuming 32bit/pixel and 4 color components.
   * @param number srcWidth
   *        The source pixel data width.
   * @param number srcHeight
   *        The source pixel data height.
   * @param number dstHeight [optional]
   *        The desired resized pixel data height.
   * @return object
   *         An objet containing the resized pixels width, height and data,
   *         represented as an 8-bit array buffer of r, g, b, a values.
   */
  resizePixels: function (srcPixels, srcWidth, srcHeight, dstHeight) {
    let screenshotRatio = dstHeight / srcHeight;
    let dstWidth = (srcWidth * screenshotRatio) | 0;
    let dstPixels = new Uint32Array(dstWidth * dstHeight);

    // If the resized image ends up being completely transparent, returning
    // an empty array will skip some redundant serialization cycles.
    let isTransparent = true;

    for (let dstX = 0; dstX < dstWidth; dstX++) {
      for (let dstY = 0; dstY < dstHeight; dstY++) {
        let srcX = (dstX / screenshotRatio) | 0;
        let srcY = (dstY / screenshotRatio) | 0;
        let cPos = srcX + srcWidth * srcY;
        let dPos = dstX + dstWidth * dstY;
        let color = dstPixels[dPos] = srcPixels[cPos];
        if (color) {
          isTransparent = false;
        }
      }
    }

    return {
      width: dstWidth,
      height: dstHeight,
      pixels: isTransparent ? [] : new Uint8Array(dstPixels.buffer)
    };
  },

  /**
   * Invokes a series of canvas context calls, to "replay" an animation frame
   * and generate a screenshot.
   *
   * In case of a WebGL context, an offscreen framebuffer is created for
   * the respective canvas, and the rendering will be performed into it.
   * This is necessary because some state (like shaders, textures etc.) can't
   * be shared between two different WebGL contexts.
   *   - Hopefully, once SharedResources are a thing this won't be necessary:
   *     http://www.khronos.org/webgl/wiki/SharedResouces
   *   - Alternatively, we could pursue the idea of using the same context
   *     for multiple canvases, instead of trying to share resources:
   *     https://www.khronos.org/webgl/public-mailing-list/archives/1210/msg00058.html
   *
   * In case of a 2D context, a new canvas is created, since there's no
   * intrinsic state that can't be easily duplicated.
   *
   * @param number contexType
   *        The type of context to use. See the CallWatcherFront scope types.
   * @param HTMLCanvasElement canvas
   *        The canvas element which is the source of all context calls.
   * @param array calls
   *        An array of function call actors.
   * @param number first
   *        The first function call to start from.
   * @param number last
   *        The last (inclusive) function call to end at.
   * @return object
   *         The context on which the specified calls were invoked, the
   *         last registered draw call's index and a cleanup function, which
   *         needs to be called whenever any potential followup work is finished.
   */
  replayAnimationFrame: function ({ contextType, canvas, calls, first, last }) {
    let w = canvas.width;
    let h = canvas.height;

    let replayContext;
    let replayContextScaling;
    let customViewport;
    let customFramebuffer;
    let lastDrawCallIndex = -1;
    let doCleanup = () => {};

    // In case of WebGL contexts, rendering will be done offscreen, in a
    // custom framebuffer, but using the same provided context. This is
    // necessary because it's very memory-unfriendly to rebuild all the
    // required GL state (like recompiling shaders, setting global flags, etc.)
    // in an entirely new canvas. However, special care is needed to not
    // permanently affect the existing GL state in the process.
    if (contextType == "WebGLRenderingContext") {
      // To keep things fast, replay the context calls on a framebuffer
      // of smaller dimensions than the actual canvas (maximum 256x256 pixels).
      let scaling = Math.min(CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT, h) / h;
      replayContextScaling = scaling;
      w = (w * scaling) | 0;
      h = (h * scaling) | 0;

      // Fetch the same WebGL context and bind a new framebuffer.
      let gl = replayContext = this.getWebGLContext(canvas);
      let { newFramebuffer, oldFramebuffer } = this.createBoundFramebuffer(gl, w, h);
      customFramebuffer = newFramebuffer;

      // Set the viewport to match the new framebuffer's dimensions.
      let { newViewport, oldViewport } = this.setCustomViewport(gl, w, h);
      customViewport = newViewport;

      // Revert the framebuffer and viewport to the original values.
      doCleanup = () => {
        gl.bindFramebuffer(gl.FRAMEBUFFER, oldFramebuffer);
        gl.viewport.apply(gl, oldViewport);
      };
    }
    // In case of 2D contexts, draw everything on a separate canvas context.
    else if (contextType == "CanvasRenderingContext2D") {
      let contentDocument = canvas.ownerDocument;
      let replayCanvas = contentDocument.createElement("canvas");
      replayCanvas.width = w;
      replayCanvas.height = h;
      replayContext = replayCanvas.getContext("2d");
      replayContextScaling = 1;
      customViewport = [0, 0, w, h];
    }

    // Replay all the context calls up to and including the specified one.
    for (let i = first; i <= last; i++) {
      let { type, name, args } = calls[i].details;

      // Prevent WebGL context calls that try to reset the framebuffer binding
      // to the default value, since we want to perform the rendering offscreen.
      if (name == "bindFramebuffer" && args[1] == null) {
        replayContext.bindFramebuffer(replayContext.FRAMEBUFFER, customFramebuffer);
        continue;
      }
      // Also prevent WebGL context calls that try to change the viewport
      // while our custom framebuffer is bound.
      if (name == "viewport") {
        let framebufferBinding = replayContext.getParameter(replayContext.FRAMEBUFFER_BINDING);
        if (framebufferBinding == customFramebuffer) {
          replayContext.viewport.apply(replayContext, customViewport);
          continue;
        }
      }
      if (type == CallWatcherFront.METHOD_FUNCTION) {
        replayContext[name].apply(replayContext, args);
      } else if (type == CallWatcherFront.SETTER_FUNCTION) {
        replayContext[name] = args;
      }
      if (CanvasFront.DRAW_CALLS.has(name)) {
        lastDrawCallIndex = i;
      }
    }

    return {
      replayContext: replayContext,
      replayContextScaling: replayContextScaling,
      replayViewport: customViewport,
      lastDrawCallIndex: lastDrawCallIndex,
      doCleanup: doCleanup
    };
  },

  /**
   * Gets an object containing a buffer large enough to hold width * height
   * pixels, assuming 32bit/pixel and 4 color components.
   *
   * This method avoids allocating memory and tries to reuse a common buffer
   * as much as possible.
   *
   * @param number w
   *        The desired pixel array storage width.
   * @param number h
   *        The desired pixel array storage height.
   * @return object
   *         The requested pixel array buffer.
   */
  getPixelStorage: function (w = 0, h = 0) {
    let storage = this._currentPixelStorage;
    if (storage && storage["32bit"].length >= w * h) {
      return storage;
    }
    return this.usePixelStorage(new ArrayBuffer(w * h * 4));
  },

  /**
   * Creates and saves the array buffer views used by `getPixelStorage`.
   *
   * @param ArrayBuffer buffer
   *        The raw buffer used as storage for various array buffer views.
   */
  usePixelStorage: function (buffer) {
    let array8bit = new Uint8Array(buffer);
    let array32bit = new Uint32Array(buffer);
    return this._currentPixelStorage = {
      "8bit": array8bit,
      "32bit": array32bit
    };
  },

  /**
   * Creates a framebuffer of the specified dimensions for a WebGL context,
   * assuming a RGBA color buffer, a depth buffer and no stencil buffer.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context to create and bind a framebuffer for.
   * @param number width
   *        The desired width of the renderbuffers.
   * @param number height
   *        The desired height of the renderbuffers.
   * @return WebGLFramebuffer
   *         The generated framebuffer object.
   */
  createBoundFramebuffer: function (gl, width, height) {
    let oldFramebuffer = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    let oldRenderbufferBinding = gl.getParameter(gl.RENDERBUFFER_BINDING);
    let oldTextureBinding = gl.getParameter(gl.TEXTURE_BINDING_2D);

    let newFramebuffer = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, newFramebuffer);

    // Use a texture as the color renderbuffer attachment, since consumers of
    // this function will most likely want to read the rendered pixels back.
    let colorBuffer = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, colorBuffer);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

    let depthBuffer = gl.createRenderbuffer();
    gl.bindRenderbuffer(gl.RENDERBUFFER, depthBuffer);
    gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT16, width, height);

    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, colorBuffer, 0);
    gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, depthBuffer);

    gl.bindTexture(gl.TEXTURE_2D, oldTextureBinding);
    gl.bindRenderbuffer(gl.RENDERBUFFER, oldRenderbufferBinding);

    return { oldFramebuffer, newFramebuffer };
  },

  /**
   * Sets the viewport of the drawing buffer for a WebGL context.
   * @param WebGLRenderingContext gl
   * @param number width
   * @param number height
   */
  setCustomViewport: function (gl, width, height) {
    let oldViewport = XPCNativeWrapper.unwrap(gl.getParameter(gl.VIEWPORT));
    let newViewport = [0, 0, width, height];
    gl.viewport.apply(gl, newViewport);

    return { oldViewport, newViewport };
  }
};

/**
 * The corresponding Front object for the CanvasActor.
 */
var CanvasFront = exports.CanvasFront = protocol.FrontClass(CanvasActor, {
  initialize: function (client, { canvasActor }) {
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
CanvasFront.THUMBNAIL_SIZE = 50; // px
CanvasFront.WEBGL_SCREENSHOT_MAX_HEIGHT = 256; // px
CanvasFront.INVALID_SNAPSHOT_IMAGE = {
  index: -1,
  width: 0,
  height: 0,
  pixels: []
};

/**
 * Goes through all the arguments and creates a one-level shallow copy
 * of all arrays and array buffers.
 */
function inplaceShallowCloneArrays(functionArguments, contentWindow) {
  let { Object, Array, ArrayBuffer } = contentWindow;

  functionArguments.forEach((arg, index, store) => {
    if (arg instanceof Array) {
      store[index] = arg.slice();
    }
    if (arg instanceof Object && arg.buffer instanceof ArrayBuffer) {
      store[index] = new arg.constructor(arg);
    }
  });
}
