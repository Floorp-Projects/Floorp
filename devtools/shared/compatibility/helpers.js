/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file might be required from a node script (./bin/update.js), so don't use
// Chrome API here.

/**
 * Return the compatibility table from given compatNode and specified terms.
 * For example, if the terms is ["background-color"],
 * this function returns compatNode["background-color"].__compat.
 *
 * @return {Object} compatibility table
 *   {
 *     description: {String} Description of this compatibility table.
 *     mdn_url: {String} Document in the MDN.
 *     support: {
 *       $browserName: {String} $browserName is such as firefox, firefox_android and so on.
 *         [
 *           {
 *              added: {String}
 *                The version this feature was added.
 *              removed: {String} Optional.
 *                The version this feature was removed. Optional.
 *              prefix: {String} Optional.
 *                The prefix this feature is needed such as "-moz-".
 *              alternative_name: {String} Optional.
 *                The alternative name of this feature such as "-moz-osx-font-smoothing" of "font-smooth".
 *              notes: {String} Optional.
 *                A simple note for this support.
 *           },
 *           ...
 *         ],
 *    },
 *    status: {
 *      experimental: {Boolean} If true, this feature is experimental.
 *      standard_track: {Boolean}, If true, this feature is on the standard track.
 *      deprecated: {Boolean} If true, this feature is deprecated.
 *    }
 *  }
 */
function getCompatTable(compatNode, terms) {
  let targetNode = getCompatNode(compatNode, terms);

  if (!targetNode) {
    return null;
  }

  if (!targetNode.__compat) {
    for (const field in targetNode) {
      // TODO: We don't have a way to know the context for now.
      // Thus, use first context node as the compat table.
      // e.g. flex_context of align-item
      // https://github.com/mdn/browser-compat-data/blob/master/css/properties/align-items.json#L5
      if (field.endsWith("_context")) {
        targetNode = targetNode[field];
        break;
      }
    }
  }

  return targetNode.__compat;
}

/**
 * Return a compatibility node which is target for `terms` parameter from `compatNode`
 * parameter. For example, when check `background-clip:  content-box;`, the `terms` will
 * be ["background-clip", "content-box"]. Then, follow the name of terms from the
 * compatNode node, return the target node. Although this function actually do more
 * complex a bit, if it says simply, returns a node of
 * compatNode["background-clip"]["content-box""] .
 */
function getCompatNode(compatNode, terms) {
  for (const term of terms) {
    compatNode = getChildCompatNode(compatNode, term);
    if (!compatNode) {
      return null;
    }
  }

  return compatNode;
}

function getChildCompatNode(compatNode, term) {
  term = term.toLowerCase();

  let child = null;
  for (const field in compatNode) {
    if (field.toLowerCase() === term) {
      child = compatNode[field];
      break;
    }
  }

  if (!child) {
    return null;
  }

  if (child._aliasOf) {
    // If the node is an alias, returns the node the alias points.
    child = compatNode[child._aliasOf];
  }

  return child;
}

module.exports = {
  getCompatNode,
  getCompatTable,
};
