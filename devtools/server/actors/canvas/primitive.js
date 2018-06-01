/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const WebGLPrimitivesType = {
  "POINTS": 0,
  "LINES": 1,
  "LINE_LOOP": 2,
  "LINE_STRIP": 3,
  "TRIANGLES": 4,
  "TRIANGLE_STRIP": 5,
  "TRIANGLE_FAN": 6
};

/**
 * A utility for monitoring WebGL primitive draws. Takes a `tabActor`
 * and monitors primitive draws over time.
 */
const WebGLDrawArrays = "drawArrays";
const WebGLDrawElements = "drawElements";

exports.WebGLPrimitiveCounter = class WebGLPrimitiveCounter {
  constructor(tabActor) {
    this.tabActor = tabActor;
  }

  destroy() {}

  /**
   * Starts monitoring primitive draws, storing the primitives count per tick.
   */
  resetCounts() {
    this._tris = 0;
    this._vertices = 0;
    this._points = 0;
    this._lines = 0;
    this._startTime = this.tabActor.docShell.now();
  }

  /**
   * Stops monitoring primitive draws, returning the recorded values.
   */
  getCounts() {
    const result = {
      tris: this._tris,
      vertices: this._vertices,
      points: this._points,
      lines: this._lines
    };

    this._tris = 0;
    this._vertices = 0;
    this._points = 0;
    this._lines = 0;
    return result;
  }

  /**
   * Handles WebGL draw primitive functions to catch primitive info.
   */
  handleDrawPrimitive(functionCall) {
    const { name, args } = functionCall.details;

    if (name === WebGLDrawArrays) {
      this._processDrawArrays(args);
    } else if (name === WebGLDrawElements) {
      this._processDrawElements(args);
    }
  }

  /**
   * Processes WebGL drawArrays method to count primitve numbers
   */
  _processDrawArrays(args) {
    const mode = args[0];
    const count = args[2];

    switch (mode) {
      case WebGLPrimitivesType.POINTS:
        this._vertices += count;
        this._points += count;
        break;
      case WebGLPrimitivesType.LINES:
        this._vertices += count;
        this._lines += (count / 2);
        break;
      case WebGLPrimitivesType.LINE_LOOP:
        this._vertices += count;
        this._lines += count;
        break;
      case WebGLPrimitivesType.LINE_STRIP:
        this._vertices += count;
        this._lines += (count - 1);
        break;
      case WebGLPrimitivesType.TRIANGLES:
        this._tris += (count / 3);
        this._vertices += count;
        break;
      case WebGLPrimitivesType.TRIANGLE_STRIP:
        this._tris += (count - 2);
        this._vertices += count;
        break;
      case WebGLPrimitivesType.TRIANGLE_FAN:
        this._tris += (count - 2);
        this._vertices += count;
        break;
      default:
        console.error("_processDrawArrays doesn't define this type.");
        break;
    }
  }

  /**
   * Processes WebGL drawElements method to count primitve numbers
   */
  _processDrawElements(args) {
    const mode = args[0];
    const count = args[1];

    switch (mode) {
      case WebGLPrimitivesType.POINTS:
        this._vertices += count;
        this._points += count;
        break;
      case WebGLPrimitivesType.LINES:
        this._vertices += count;
        this._lines += (count / 2);
        break;
      case WebGLPrimitivesType.LINE_LOOP:
        this._vertices += count;
        this._lines += count;
        break;
      case WebGLPrimitivesType.LINE_STRIP:
        this._vertices += count;
        this._lines += (count - 1);
        break;
      case WebGLPrimitivesType.TRIANGLES:
        const tris = count / 3;
        let vertex = tris * 3;

        if (tris > 1) {
          vertex = tris * 2;
        }
        this._tris += tris;
        this._vertices += vertex;
        break;
      case WebGLPrimitivesType.TRIANGLE_STRIP:
        this._tris += (count - 2);
        this._vertices += count;
        break;
      case WebGLPrimitivesType.TRIANGLE_FAN:
        this._tris += (count - 2);
        this._vertices += count;
        break;
      default:
        console.error("_processDrawElements doesn't define this type.");
        break;
    }
  }
};
