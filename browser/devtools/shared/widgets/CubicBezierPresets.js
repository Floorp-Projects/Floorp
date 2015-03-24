/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Set of preset definitions for use with CubicBezierWidget
// Credit: http://easings.net

"use strict";

const PREDEFINED = {
  "ease": [0.25, 0.1, 0.25, 1],
  "linear": [0, 0, 1, 1],
  "ease-in": [0.42, 0, 1, 1],
  "ease-out": [0, 0, 0.58, 1],
  "ease-in-out": [0.42, 0, 0.58, 1]
};

const PRESETS = {
  "ease-in": {
    "ease-in-linear": [0, 0, 1, 1],
    "ease-in-ease-in": [0.42, 0, 1, 1],
    "ease-in-sine": [0.47, 0, 0.74, 0.71],
    "ease-in-quadratic": [0.55, 0.09, 0.68, 0.53],
    "ease-in-cubic": [0.55, 0.06, 0.68, 0.19],
    "ease-in-quartic": [0.9, 0.03, 0.69, 0.22],
    "ease-in-quintic": [0.76, 0.05, 0.86, 0.06],
    "ease-in-exponential": [0.95, 0.05, 0.8, 0.04],
    "ease-in-circular": [0.6, 0.04, 0.98, 0.34],
    "ease-in-backward": [0.6, -0.28, 0.74, 0.05]
  },
  "ease-out": {
    "ease-out-linear": [0, 0, 1, 1],
    "ease-out-ease-out": [0, 0, 0.58, 1],
    "ease-out-sine": [0.39, 0.58, 0.57, 1],
    "ease-out-quadratic": [0.25, 0.46, 0.45, 0.94],
    "ease-out-cubic": [0.22, 0.61, 0.36, 1],
    "ease-out-quartic": [0.17, 0.84, 0.44, 1],
    "ease-out-quintic": [0.23, 1, 0.32, 1],
    "ease-out-exponential": [0.19, 1, 0.22, 1],
    "ease-out-circular": [0.08, 0.82, 0.17, 1],
    "ease-out-backward": [0.18, 0.89, 0.32, 1.28]
  },
  "ease-in-out": {
    "ease-in-out-linear": [0, 0, 1, 1],
    "ease-in-out-ease": [0.25, 0.1, 0.25, 1],
    "ease-in-out-ease-in-out": [0.42, 0, 0.58, 1],
    "ease-in-out-sine": [0.45, 0.05, 0.55, 0.95],
    "ease-in-out-quadratic": [0.46, 0.03, 0.52, 0.96],
    "ease-in-out-cubic": [0.65, 0.05, 0.36, 1],
    "ease-in-out-quartic": [0.77, 0, 0.18, 1],
    "ease-in-out-quintic": [0.86, 0, 0.07, 1],
    "ease-in-out-exponential": [1, 0, 0, 1],
    "ease-in-out-circular": [0.79, 0.14, 0.15, 0.86],
    "ease-in-out-backward": [0.68, -0.55, 0.27, 1.55]
  }
};

const DEFAULT_PRESET_CATEGORY = Object.keys(PRESETS)[0];

exports.PRESETS = PRESETS;
exports.PREDEFINED = PREDEFINED;
exports.DEFAULT_PRESET_CATEGORY = DEFAULT_PRESET_CATEGORY;
