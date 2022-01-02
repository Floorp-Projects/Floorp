/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Util base class to help test a captured canvas element. Initializes the
 * output canvas (used for testing the color of video elements), and optionally
 * overrides the default `createAndAppendElement` element |width| and |height|.
 */
function CaptureStreamTestHelper(width, height) {
  if (width) {
    this.elemWidth = width;
  }
  if (height) {
    this.elemHeight = height;
  }

  /* cout is used for `getPixel`; only needs to be big enough for one pixel */
  this.cout = document.createElement("canvas");
  this.cout.width = 1;
  this.cout.height = 1;
}

CaptureStreamTestHelper.prototype = {
  /* Predefined colors for use in the methods below. */
  black: { data: [0, 0, 0, 255], name: "black" },
  blackTransparent: { data: [0, 0, 0, 0], name: "blackTransparent" },
  green: { data: [0, 255, 0, 255], name: "green" },
  red: { data: [255, 0, 0, 255], name: "red" },
  blue: { data: [0, 0, 255, 255], name: "blue" },
  grey: { data: [128, 128, 128, 255], name: "grey" },

  /* Default element size for createAndAppendElement() */
  elemWidth: 100,
  elemHeight: 100,

  /*
   * Perform the drawing operation on each animation frame until stop is called
   * on the returned object.
   */
  startDrawing(f) {
    var stop = false;
    var draw = () => {
      if (stop) {
        return;
      }
      f();
      window.requestAnimationFrame(draw);
    };
    draw();
    return { stop: () => (stop = true) };
  },

  /* Request a frame from the stream played by |video|. */
  requestFrame(video) {
    info("Requesting frame from " + video.id);
    video.srcObject.requestFrame();
  },

  /*
   * Returns the pixel at (|offsetX|, |offsetY|) (from top left corner) of
   * |video| as an array of the pixel's color channels: [R,G,B,A].
   */
  getPixel(video, offsetX = 0, offsetY = 0) {
    // Avoids old values in case of a transparent image.
    CaptureStreamTestHelper2D.prototype.clear.call(this, this.cout);

    var ctxout = this.cout.getContext("2d");
    ctxout.drawImage(
      video,
      offsetX, // source x coordinate
      offsetY, // source y coordinate
      1, // source width
      1, // source height
      0, // destination x coordinate
      0, // destination y coordinate
      1, // destination width
      1
    ); // destination height
    return ctxout.getImageData(0, 0, 1, 1).data;
  },

  /*
   * Returns true if px lies within the per-channel |threshold| of the
   * referenced color for all channels. px is on the form of an array of color
   * channels, [R,G,B,A]. Each channel is in the range [0, 255].
   *
   * Threshold defaults to 0 which is an exact match.
   */
  isPixel(px, refColor, threshold = 0) {
    return px.every((ch, i) => Math.abs(ch - refColor.data[i]) <= threshold);
  },

  /*
   * Returns true if px lies further away than |threshold| of the
   * referenced color for any channel. px is on the form of an array of color
   * channels, [R,G,B,A]. Each channel is in the range [0, 255].
   *
   * Threshold defaults to 127 which should be far enough for most cases.
   */
  isPixelNot(px, refColor, threshold = 127) {
    return px.some((ch, i) => Math.abs(ch - refColor.data[i]) > threshold);
  },

  /*
   * Behaves like isPixelNot but ignores the alpha channel.
   */
  isOpaquePixelNot(px, refColor, threshold) {
    px[3] = refColor.data[3];
    return this.isPixelNot(px, refColor, threshold);
  },

  /*
   * Returns a promise that resolves when the provided function |test|
   * returns true, or rejects when the optional `cancel` promise resolves.
   */
  async waitForPixel(
    video,
    test,
    {
      offsetX = 0,
      offsetY = 0,
      width = 0,
      height = 0,
      cancel = new Promise(() => {}),
    } = {}
  ) {
    let aborted = false;
    cancel.then(e => (aborted = true));

    while (true) {
      await Promise.race([
        new Promise(resolve =>
          video.addEventListener("timeupdate", resolve, { once: true })
        ),
        cancel,
      ]);
      if (aborted) {
        throw await cancel;
      }
      if (test(this.getPixel(video, offsetX, offsetY, width, height))) {
        return;
      }
    }
  },

  /*
   * Returns a promise that resolves when the top left pixel of |video| matches
   * on all channels. Use |threshold| for fuzzy matching the color on each
   * channel, in the range [0,255]. 0 means exact match, 255 accepts anything.
   */
  async pixelMustBecome(
    video,
    refColor,
    { threshold = 0, infoString = "n/a", cancel = new Promise(() => {}) } = {}
  ) {
    info(
      "Waiting for video " +
        video.id +
        " to match [" +
        refColor.data.join(",") +
        "] - " +
        refColor.name +
        " (" +
        infoString +
        ")"
    );
    var paintedFrames = video.mozPaintedFrames - 1;
    await this.waitForPixel(
      video,
      px => {
        if (paintedFrames != video.mozPaintedFrames) {
          info(
            "Frame: " +
              video.mozPaintedFrames +
              " IsPixel ref=" +
              refColor.data +
              " threshold=" +
              threshold +
              " value=" +
              px
          );
          paintedFrames = video.mozPaintedFrames;
        }
        return this.isPixel(px, refColor, threshold);
      },
      {
        offsetX: 0,
        offsetY: 0,
        width: 0,
        height: 0,
        cancel,
      }
    );
    ok(true, video.id + " " + infoString);
  },

  /*
   * Returns a promise that resolves after |time| ms of playback or when the
   * top left pixel of |video| becomes |refColor|. The test is failed if the
   * time is not reached, or if the cancel promise resolves.
   */
  async pixelMustNotBecome(
    video,
    refColor,
    { threshold = 0, time = 5000, infoString = "n/a" } = {}
  ) {
    info(
      "Waiting for " +
        video.id +
        " to time out after " +
        time +
        "ms against [" +
        refColor.data.join(",") +
        "] - " +
        refColor.name
    );
    let timeout = new Promise(resolve => setTimeout(resolve, time));
    let analysis = async () => {
      await this.waitForPixel(
        video,
        px => this.isPixel(px, refColor, threshold),
        {
          offsetX: 0,
          offsetY: 0,
          width: 0,
          height: 0,
        }
      );
      throw new Error("Got color " + refColor.name + ". " + infoString);
    };
    await Promise.race([timeout, analysis()]);
    ok(true, video.id + " " + infoString);
  },

  /* Create an element of type |type| with id |id| and append it to the body. */
  createAndAppendElement(type, id) {
    var e = document.createElement(type);
    e.id = id;
    e.width = this.elemWidth;
    e.height = this.elemHeight;
    if (type === "video") {
      e.autoplay = true;
    }
    document.body.appendChild(e);
    return e;
  },
};

/* Sub class holding 2D-Canvas specific helpers. */
function CaptureStreamTestHelper2D(width, height) {
  CaptureStreamTestHelper.call(this, width, height);
}

CaptureStreamTestHelper2D.prototype = Object.create(
  CaptureStreamTestHelper.prototype
);
CaptureStreamTestHelper2D.prototype.constructor = CaptureStreamTestHelper2D;

/* Clear all drawn content on |canvas|. */
CaptureStreamTestHelper2D.prototype.clear = function(canvas) {
  var ctx = canvas.getContext("2d");
  ctx.clearRect(0, 0, canvas.width, canvas.height);
};

/* Draw the color |color| to the source canvas |canvas|. Format [R,G,B,A]. */
CaptureStreamTestHelper2D.prototype.drawColor = function(
  canvas,
  color,
  {
    offsetX = 0,
    offsetY = 0,
    width = canvas.width / 2,
    height = canvas.height / 2,
  } = {}
) {
  var ctx = canvas.getContext("2d");
  var rgba = color.data.slice(); // Copy to not overwrite the original array
  rgba[3] = rgba[3] / 255.0; // Convert opacity to double in range [0,1]
  info("Drawing color " + rgba.join(","));
  ctx.fillStyle = "rgba(" + rgba.join(",") + ")";

  // Only fill top left corner to test that output is not flipped or rotated.
  ctx.fillRect(offsetX, offsetY, width, height);
};

/* Test that the given 2d canvas is NOT origin-clean. */
CaptureStreamTestHelper2D.prototype.testNotClean = function(canvas) {
  var ctx = canvas.getContext("2d");
  var error = "OK";
  try {
    var data = ctx.getImageData(0, 0, 1, 1);
  } catch (e) {
    error = e.name;
  }
  is(
    error,
    "SecurityError",
    "Canvas '" + canvas.id + "' should not be origin-clean"
  );
};

/* Sub class holding WebGL specific helpers. */
function CaptureStreamTestHelperWebGL(width, height) {
  CaptureStreamTestHelper.call(this, width, height);
}

CaptureStreamTestHelperWebGL.prototype = Object.create(
  CaptureStreamTestHelper.prototype
);
CaptureStreamTestHelperWebGL.prototype.constructor = CaptureStreamTestHelperWebGL;

/* Set the (uniform) color location for future draw calls. */
CaptureStreamTestHelperWebGL.prototype.setFragmentColorLocation = function(
  colorLocation
) {
  this.colorLocation = colorLocation;
};

/* Clear the given WebGL context with |color|. */
CaptureStreamTestHelperWebGL.prototype.clearColor = function(canvas, color) {
  info("WebGL: clearColor(" + color.name + ")");
  var gl = canvas.getContext("webgl");
  var conv = color.data.map(i => i / 255.0);
  gl.clearColor(conv[0], conv[1], conv[2], conv[3]);
  gl.clear(gl.COLOR_BUFFER_BIT);
};

/* Set an already setFragmentColorLocation() to |color| and drawArrays() */
CaptureStreamTestHelperWebGL.prototype.drawColor = function(canvas, color) {
  info("WebGL: drawArrays(" + color.name + ")");
  var gl = canvas.getContext("webgl");
  var conv = color.data.map(i => i / 255.0);
  gl.uniform4f(this.colorLocation, conv[0], conv[1], conv[2], conv[3]);
  gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
};
