/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Transformer for the root node resource.
 *
 * @param {NodeFront} resource
 * @param {TargetFront} targetFront
 * @return {NodeFront} the updated resource
 */
module.exports = function({ resource, targetFront }) {
  if (!resource.traits.supportsIsTopLevelDocument) {
    // When `supportsIsTopLevelDocument` is false, a root-node resource is
    // necessarily top level, se we can fallback to true.
    resource.isTopLevelDocument = true;
  }
  return resource;
};
