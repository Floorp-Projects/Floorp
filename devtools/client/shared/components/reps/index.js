/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  MODE,
} = require("devtools/client/shared/components/reps/reps/constants");
const {
  REPS,
  getRep,
} = require("devtools/client/shared/components/reps/reps/rep");
const objectInspector = require("devtools/client/shared/components/object-inspector/index");

const {
  parseURLEncodedText,
  parseURLParams,
  maybeEscapePropertyName,
  getGripPreviewItems,
} = require("devtools/client/shared/components/reps/reps/rep-utils");

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
