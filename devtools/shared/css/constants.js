/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * All CSS <angle> types that properties can support.
 */
exports.CSS_ANGLEUNIT = {
  deg: "deg",
  rad: "rad",
  grad: "grad",
  turn: "turn",
};

/**
 * Mapping of InspectorPropertyType to old type ID.
 * Kept for backwards compatibility. Remove after Firefox 70.
 */
exports.CSS_TYPES = {
  color: 2,
  gradient: 4,
  "timing-function": 10,
};

/**
 * Supported pseudo-class locks in the order in which they appear in the pseudo-class
 * panel in the Rules sidebar panel of the Inspector.
 * FIXME: add ":visited" and ":link" after bug 713106 is fixed
 */
exports.PSEUDO_CLASSES = [":hover", ":active", ":focus", ":focus-within"];
