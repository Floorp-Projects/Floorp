/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Compares 2 sets of grid fragments to each other and checks if they have the same
 * general geometry.
 * This means that things like areas, area names or line names are ignored.
 * This only checks if the 2 sets of fragments have as many fragments, as many lines, and
 * that those lines are at the same distance.
 *
 * @param  {Array} fragments1
 *         A list of gridFragment objects.
 * @param  {Array} fragments2
 *         Another list of gridFragment objects to compare to the first list.
 * @return {Boolean}
 *         True if the fragments are the same, false otherwise.
 */
function compareFragmentsGeometry(fragments1, fragments2) {
  // Compare the number of fragments.
  if (fragments1.length !== fragments2.length) {
    return false;
  }

  // Compare the number of areas, rows and columns.
  for (let i = 0; i < fragments1.length; i++) {
    if (fragments1[i].cols.lines.length !== fragments2[i].cols.lines.length ||
        fragments1[i].rows.lines.length !== fragments2[i].rows.lines.length) {
      return false;
    }
  }

  // Compare the offset of lines.
  for (let i = 0; i < fragments1.length; i++) {
    for (let j = 0; j < fragments1[i].cols.lines.length; j++) {
      if (fragments1[i].cols.lines[j].start !== fragments2[i].cols.lines[j].start) {
        return false;
      }
    }
    for (let j = 0; j < fragments1[i].rows.lines.length; j++) {
      if (fragments1[i].rows.lines[j].start !== fragments2[i].rows.lines[j].start) {
        return false;
      }
    }
  }

  return true;
}

module.exports.compareFragmentsGeometry = compareFragmentsGeometry;
