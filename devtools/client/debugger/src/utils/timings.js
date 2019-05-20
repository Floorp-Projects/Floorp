/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { zip } from "lodash";

export function getAsyncTimes(name: string): number[] {
  return zip(
    window.performance.getEntriesByName(`${name}_start`),
    window.performance.getEntriesByName(`${name}_end`)
  ).map(([start, end]) => +(end.startTime - start.startTime).toPrecision(2));
}

function getTimes(name) {
  return window.performance
    .getEntriesByName(name)
    .map(time => +time.duration.toPrecision(2));
}

function getStats(times) {
  if (times.length == 0) {
    return { times: [], avg: null, median: null };
  }
  const avg = times.reduce((sum, time) => time + sum, 0) / times.length;
  const sortedtimings = [...times].sort((a, b) => a - b);
  const median = sortedtimings[times.length / 2];
  return {
    times,
    avg: +avg.toPrecision(2),
    median: +median.toPrecision(2),
  };
}

export function steppingTimings() {
  const commandTimings = getAsyncTimes("COMMAND");
  const pausedTimings = getTimes("PAUSED");

  return {
    commands: getStats(commandTimings),
    paused: getStats(pausedTimings),
  };
}

// console.log("..", asyncTimes("COMMAND"));
