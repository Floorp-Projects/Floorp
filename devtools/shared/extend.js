/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Utility function, that is useful for creating objects that inherit from other
 * objects, without associated classes.
 *
 * Replacement for `extends` API from "sdk/core/heritage".
 */
exports.extend = function (prototype, properties) {
  return Object.create(prototype, Object.getOwnPropertyDescriptors(properties));
};
