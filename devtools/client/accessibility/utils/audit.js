/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A helper class that contains the state of the audit progress performed by
 * the accessibility panel. Its onProgressForWalker function wraps around the
 * onProgress function (see actions/audit.js) that updates the panel state. It
 * combines the audits across multiple frames that happen asynchronously. It
 * only starts dispatching/calling onProgress after we get an initial progress
 * audit-event from all frames (and thus know the combined total).
 */
class CombinedProgress {
  constructor({ onProgress, totalFrames }) {
    this.onProgress = onProgress;
    this.totalFrames = totalFrames;
    this.combinedProgress = new Map();
  }

  onProgressForWalker(walker, progress) {
    this.combinedProgress.set(walker, progress);
    // We did not get all initial progress events from all frames, do not
    // relay them to the client until we can calculate combined total below.
    if (this.combinedProgress.size < this.totalFrames) {
      return;
    }

    let combinedTotal = 0;
    let combinedCompleted = 0;

    for (const { completed, total } of this.combinedProgress.values()) {
      combinedTotal += total;
      combinedCompleted += completed;
    }
    this.onProgress({
      total: combinedTotal,
      percentage: Math.round((combinedCompleted / combinedTotal) * 100),
    });
  }
}

exports.CombinedProgress = CombinedProgress;
exports.isFiltered = filters => Object.values(filters).some(active => active);
