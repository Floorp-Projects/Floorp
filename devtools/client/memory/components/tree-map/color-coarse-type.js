/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Color the boxes in the treemap
 */

const TYPES = [ "objects", "other", "strings", "scripts" ];

// The factors determine how much the hue shifts
const TYPE_FACTOR = TYPES.length * 3;
const DEPTH_FACTOR = -10;
const H = 0.5;
const S = 0.6;
const L = 0.9;

/**
 * Recursively find the index of the coarse type of a node
 *
 * @param  {Object} node
 *         d3 treemap
 * @return {Integer}
 *         index
 */
function findCoarseTypeIndex(node) {
  let index = TYPES.indexOf(node.name);

  if (node.parent) {
    return index === -1 ? findCoarseTypeIndex(node.parent) : index;
  }

  return TYPES.indexOf("other");
}

/**
 * Decide a color value for depth to be used in the HSL computation
 *
 * @param  {Object} node
 * @return {Number}
 */
function depthColorFactor(node) {
  return Math.min(1, node.depth / DEPTH_FACTOR);
}

/**
 * Decide a color value for type to be used in the HSL computation
 *
 * @param  {Object} node
 * @return {Number}
 */
function typeColorFactor(node) {
  return findCoarseTypeIndex(node) / TYPE_FACTOR;
}

/**
 * Color a node
 *
 * @param  {Object} node
 * @return {Array} HSL values ranged 0-1
 */
module.exports = function colorCoarseType(node) {
  let h = Math.min(1, H + typeColorFactor(node));
  let s = Math.min(1, S);
  let l = Math.min(1, L + depthColorFactor(node));

  return [h, s, l];
};
