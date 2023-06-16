/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  accessibility: {
    SIMULATION_TYPE: {
      ACHROMATOPSIA,
      DEUTERANOPIA,
      PROTANOPIA,
      TRITANOPIA,
      CONTRAST_LOSS,
    },
  },
} = require("resource://devtools/shared/constants.js");

/**
 * Constants used in accessibility actors.
 */

// Color blindness matrix values taken from Machado et al. (2009), https://doi.org/10.1109/TVCG.2009.113:
// https://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/CVD_Simulation.html
// Contrast loss matrix values are for 50% contrast (see https://docs.rainmeter.net/tips/colormatrix-guide/,
// and https://stackoverflow.com/questions/23865511/contrast-with-color-matrix). The matrices are flattened
// 4x5 matrices, needed for docShell setColorMatrix method. i.e. A 4x5 matrix of the form:
//   1  2  3  4  5
//   6  7  8  9  10
//   11 12 13 14 15
//   16 17 18 19 20
// will be need to be set in docShell as:
//   [1, 6, 11, 16, 2, 7, 12, 17, 3, 8, 13, 18, 4, 9, 14, 19, 5, 10, 15, 20]
const COLOR_TRANSFORMATION_MATRICES = {
  NONE: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0],
  [ACHROMATOPSIA]: [
    0.299, 0.299, 0.299, 0, 0.587, 0.587, 0.587, 0, 0.114, 0.114, 0.114, 0, 0,
    0, 0, 1, 0, 0, 0, 0,
  ],
  [PROTANOPIA]: [
    0.152286, 0.114503, -0.003882, 0, 1.052583, 0.786281, -0.048116, 0,
    -0.204868, 0.099216, 1.051998, 0, 0, 0, 0, 1, 0, 0, 0, 0,
  ],
  [DEUTERANOPIA]: [
    0.367322, 0.280085, -0.01182, 0, 0.860646, 0.672501, 0.04294, 0, -0.227968,
    0.047413, 0.968881, 0, 0, 0, 0, 1, 0, 0, 0, 0,
  ],
  [TRITANOPIA]: [
    1.255528, -0.078411, 0.004733, 0, -0.076749, 0.930809, 0.691367, 0,
    -0.178779, 0.147602, 0.3039, 0, 0, 0, 0, 1, 0, 0, 0, 0,
  ],
  [CONTRAST_LOSS]: [
    0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0.5, 0.25, 0.25, 0.25, 0,
  ],
};

exports.simulation = {
  COLOR_TRANSFORMATION_MATRICES,
};
