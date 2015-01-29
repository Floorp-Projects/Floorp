/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Utility functions for managing recording models and their internal data,
 * such as filtering profile samples or offsetting timestamps.
 */

exports.RecordingUtils = {};

/**
 * Filters all the samples in the provided profiler data to be more recent
 * than the specified start time.
 *
 * @param object profile
 *        The profiler data received from the backend.
 * @param number profilerStartTime
 *        The earliest acceptable sample time (in milliseconds).
 */
exports.RecordingUtils.filterSamples = function(profile, profilerStartTime) {
  let firstThread = profile.threads[0];

  firstThread.samples = firstThread.samples.filter(e => {
    return e.time >= profilerStartTime;
  });
}

/**
 * Offsets all the samples in the provided profiler data by the specified time.
 *
 * @param object profile
 *        The profiler data received from the backend.
 * @param number timeOffset
 *        The amount of time to offset by (in milliseconds).
 */
exports.RecordingUtils.offsetSampleTimes = function(profile, timeOffset) {
  let firstThread = profile.threads[0];

  for (let sample of firstThread.samples) {
    sample.time -= timeOffset;
  }
}

/**
 * Offsets all the markers in the provided timeline data by the specified time.
 *
 * @param array markers
 *        The markers array received from the backend.
 * @param number timeOffset
 *        The amount of time to offset by (in milliseconds).
 */
exports.RecordingUtils.offsetMarkerTimes = function(markers, timeOffset) {
  for (let marker of markers) {
    marker.start -= timeOffset;
    marker.end -= timeOffset;
  }
}

/**
 * Converts allocation data from the memory actor to something that follows
 * the same structure as the samples data received from the profiler.
 *
 * @see MemoryActor.prototype.getAllocations for more information.
 *
 * @param object allocations
 *        A list of { sites, timestamps, frames, counts } arrays.
 * @return array
 *         The samples data.
 */
exports.RecordingUtils.getSamplesFromAllocations = function(allocations) {
  let { sites, timestamps, frames, counts } = allocations;
  let samples = [];

  for (let i = 0, len = sites.length; i < len; i++) {
    let site = sites[i];
    let timestamp = timestamps[i];
    let frame = frames[site];
    let count = counts[site];

    let sample = { time: timestamp, frames: [] };
    samples.push(sample);

    while (frame) {
      sample.frames.push({
        location: frame.source + ":" + frame.line + ":" + frame.column,
        allocations: count
      });
      site = frame.parent;
      frame = frames[site];
      count = counts[site];
    }

    sample.frames.reverse();
  }

  return samples;
}
