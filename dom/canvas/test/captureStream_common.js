/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Util base class to help test a captured canvas element. Initializes the
 * output canvas (used for testing the color of video elements), and optionally
 * overrides the default element |width| and |height|.
 */
function CaptureStreamTestHelper(width, height) {
  this.cout = document.createElement('canvas');
  if (width) {
    this.elemWidth = width;
  }
  if (height) {
    this.elemHeight = height;
  }
  this.cout.width = this.elemWidth;
  this.cout.height = this.elemHeight;
  document.body.appendChild(this.cout);
}

CaptureStreamTestHelper.prototype = {
  /* Predefined colors for use in the methods below. */
  black: { data: [0, 0, 0, 255], name: "black" },
  green: { data: [0, 255, 0, 255], name: "green" },
  red: { data: [255, 0, 0, 255], name: "red" },

  /* Default element size for createAndAppendElement() */
  elemWidth: 100,
  elemHeight: 100,

  /*
   * Perform the drawing operation on each animation frame until stop is called
   * on the returned object.
   */
  startDrawing: function (f) {
    var stop = false;
    var draw = () => {
      f();
      if (!stop) { window.requestAnimationFrame(draw); }
    };
    draw();
    return { stop: () => stop = true };
  },

  /* Request a frame from the stream played by |video|. */
  requestFrame: function (video) {
    info("Requesting frame from " + video.id);
    video.srcObject.requestFrame();
  },

  /* Tests the top left pixel of |video| against |refData|. Format [R,G,B,A]. */
  testPixel: function (video, refData, threshold) {
    var ctxout = this.cout.getContext('2d');
    ctxout.drawImage(video, 0, 0);
    var pixel = ctxout.getImageData(0, 0, 1, 1).data;
    return pixel.every((val, i) => Math.abs(val - refData[i]) <= threshold);
  },

  /*
   * Returns a promise that resolves when the pixel matches. Use |threshold|
   * for fuzzy matching the color on each channel, in the range [0,255].
   */
  waitForPixel: function (video, refColor, threshold, infoString) {
    return new Promise(resolve => {
      info("Testing " + video.id + " against [" + refColor.data.join(',') + "]");
      CaptureStreamTestHelper2D.prototype.clear.call(this, this.cout);
      video.ontimeupdate = () => {
        if (this.testPixel(video, refColor.data, threshold)) {
          ok(true, video.id + " " + infoString);
          video.ontimeupdate = null;
          resolve();
        }
      };
    });
  },

  /*
   * Returns a promise that resolves after |timeout| ms of playback or when a
   * pixel of |video| becomes the color |refData|. The test is failed if the
   * timeout is not reached.
   */
  waitForPixelToTimeout: function (video, refColor, threshold, timeout, infoString) {
    return new Promise(resolve => {
      info("Waiting for " + video.id + " to time out after " + timeout +
           "ms against [" + refColor.data.join(',') + "] - " + refColor.name);
      CaptureStreamTestHelper2D.prototype.clear.call(this, this.cout);
      var startTime = video.currentTime;
      video.ontimeupdate = () => {
        if (this.testPixel(video, refColor.data, threshold)) {
          ok(false, video.id + " " + infoString);
          video.ontimeupdate = null;
          resolve();
        } else if (video.currentTime > startTime + (timeout / 1000.0)) {
          ok(true, video.id + " " + infoString);
          video.ontimeupdate = null;
          resolve();
        }
      };
    });
  },

  /* Create an element of type |type| with id |id| and append it to the body. */
  createAndAppendElement: function (type, id) {
    var e = document.createElement(type);
    e.id = id;
    e.width = this.elemWidth;
    e.height = this.elemHeight;
    if (type === 'video') {
      e.autoplay = true;
    }
    document.body.appendChild(e);
    return e;
  },
}

/* Sub class holding 2D-Canvas specific helpers. */
function CaptureStreamTestHelper2D(width, height) {
  CaptureStreamTestHelper.call(this, width, height);
}

CaptureStreamTestHelper2D.prototype = Object.create(CaptureStreamTestHelper.prototype);
CaptureStreamTestHelper2D.prototype.constructor = CaptureStreamTestHelper2D;

/* Clear all drawn content on |canvas|. */
CaptureStreamTestHelper2D.prototype.clear = function(canvas) {
  var ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, canvas.width, canvas.height);
};

/* Draw the color |color| to the source canvas |canvas|. Format [R,G,B,A]. */
CaptureStreamTestHelper2D.prototype.drawColor = function(canvas, color) {
  var ctx = canvas.getContext('2d');
  var rgba = color.data.slice(); // Copy to not overwrite the original array
  rgba[3] = rgba[3] / 255.0; // Convert opacity to double in range [0,1]
  info("Drawing color " + rgba.join(','));
  ctx.fillStyle = "rgba(" + rgba.join(',') + ")";

  // Only fill top left corner to test that output is not flipped or rotated.
  ctx.fillRect(0, 0, canvas.width / 2, canvas.height / 2);
};

/* Test that the given 2d canvas is NOT origin-clean. */
CaptureStreamTestHelper2D.prototype.testNotClean = function(canvas) {
  var ctx = canvas.getContext('2d');
  var error = "OK";
  try {
    var data = ctx.getImageData(0, 0, 1, 1);
  } catch(e) {
    error = e.name;
  }
  is(error, "SecurityError",
     "Canvas '" + canvas.id + "' should not be origin-clean");
};

/* Sub class holding WebGL specific helpers. */
function CaptureStreamTestHelperWebGL(width, height) {
  CaptureStreamTestHelper.call(this, width, height);
}

CaptureStreamTestHelperWebGL.prototype = Object.create(CaptureStreamTestHelper.prototype);
CaptureStreamTestHelperWebGL.prototype.constructor = CaptureStreamTestHelperWebGL;

/* Set the (uniform) color location for future draw calls. */
CaptureStreamTestHelperWebGL.prototype.setFragmentColorLocation = function(colorLocation) {
  this.colorLocation = colorLocation;
};

/* Clear the given WebGL context with |color|. */
CaptureStreamTestHelperWebGL.prototype.clearColor = function(canvas, color) {
  info("WebGL: clearColor(" + color.name + ")");
  var gl = canvas.getContext('webgl');
  var conv = color.data.map(i => i / 255.0);
  gl.clearColor(conv[0], conv[1], conv[2], conv[3]);
  gl.clear(gl.COLOR_BUFFER_BIT);
};

/* Set an already setFragmentColorLocation() to |color| and drawArrays() */
CaptureStreamTestHelperWebGL.prototype.drawColor = function(canvas, color) {
  info("WebGL: drawArrays(" + color.name + ")");
  var gl = canvas.getContext('webgl');
  var conv = color.data.map(i => i / 255.0);
  gl.uniform4f(this.colorLocation, conv[0], conv[1], conv[2], conv[3]);
  gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
};
