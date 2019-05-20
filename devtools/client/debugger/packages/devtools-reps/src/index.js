/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

const { MODE } = require("./reps/constants");
const { REPS, getRep } = require("./reps/rep");
const objectInspector = require("./object-inspector");

const {
  parseURLEncodedText,
  parseURLParams,
  maybeEscapePropertyName,
  getGripPreviewItems,
} = require("./reps/rep-utils");

module.exports = {
  REPS,
  getRep,
  MODE,
  maybeEscapePropertyName,
  parseURLEncodedText,
  parseURLParams,
  getGripPreviewItems,
  objectInspector,
};

export type {
  RdpGrip,
  GripProperties,
  Node,
  Grip,
} from "./object-inspector/types";
