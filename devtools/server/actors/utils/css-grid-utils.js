/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");

/**
 * Returns the grid fragment array with all the grid fragment data stringifiable.
 *
 * @param  {Object} fragments
 *         Grid fragment object.
 * @return {Array} representation with the grid fragment data stringifiable.
 */
function getStringifiableFragments(fragments = []) {
  if (fragments[0] && Cu.isDeadWrapper(fragments[0])) {
    return {};
  }

  return fragments.map(getStringifiableFragment);
}

/**
 * Returns a string representation of the CSS Grid data as returned by
 * node.getGridFragments. This is useful to compare grid state at each update and redraw
 * the highlighter if needed. It also seralizes the grid fragment data so it can be used
 * by protocol.js.
 *
 * @param  {Object} fragments
 *         Grid fragment object.
 * @return {String} representation of the CSS grid fragment data.
 */
function stringifyGridFragments(fragments) {
  return JSON.stringify(getStringifiableFragments(fragments));
}

function getStringifiableFragment(fragment) {
  return {
    areas: getStringifiableAreas(fragment.areas),
    cols: getStringifiableDimension(fragment.cols),
    rows: getStringifiableDimension(fragment.rows),
  };
}

function getStringifiableAreas(areas) {
  return [...areas].map(getStringifiableArea);
}

function getStringifiableDimension(dimension) {
  return {
    lines: [...dimension.lines].map(getStringifiableLine),
    tracks: [...dimension.tracks].map(getStringifiableTrack),
  };
}

function getStringifiableArea({
  columnEnd,
  columnStart,
  name,
  rowEnd,
  rowStart,
  type,
}) {
  return { columnEnd, columnStart, name, rowEnd, rowStart, type };
}

function getStringifiableLine({ breadth, names, number, start, type }) {
  return { breadth, names, number, start, type };
}

function getStringifiableTrack({ breadth, start, state, type }) {
  return { breadth, start, state, type };
}

exports.getStringifiableFragments = getStringifiableFragments;
exports.stringifyGridFragments = stringifyGridFragments;
