/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test WatchedDataHelpers.
 */

"use strict";

const { WatchedDataHelpers } = ChromeUtils.import(
  "resource://devtools/server/actors/watcher/WatchedDataHelpers.jsm"
);
const { SUPPORTED_DATA } = WatchedDataHelpers;
const { TARGETS } = SUPPORTED_DATA;

function run_test() {
  const watchedData = {
    [TARGETS]: [],
  };

  WatchedDataHelpers.addWatchedDataEntry(watchedData, TARGETS, [
    "frame",
    "worker",
  ]);
  deepEqual(
    watchedData[TARGETS],
    ["frame", "worker"],
    "the two elements were added"
  );

  WatchedDataHelpers.addWatchedDataEntry(watchedData, TARGETS, ["frame"]);
  deepEqual(
    watchedData[TARGETS],
    ["frame", "worker"],
    "addWatchedDataEntry ignore duplicates"
  );

  WatchedDataHelpers.addWatchedDataEntry(watchedData, TARGETS, ["process"]);
  deepEqual(
    watchedData[TARGETS],
    ["frame", "worker", "process"],
    "the third element is added"
  );

  let removed = WatchedDataHelpers.removeWatchedDataEntry(
    watchedData,
    TARGETS,
    ["process"]
  );
  ok(removed, "removedWatchedDataEntry returned true as it removed an element");
  deepEqual(
    watchedData[TARGETS],
    ["frame", "worker"],
    "the element has been remove"
  );

  removed = WatchedDataHelpers.removeWatchedDataEntry(watchedData, TARGETS, [
    "not-existing",
  ]);
  ok(
    !removed,
    "removedWatchedDataEntry returned false as no element has been removed"
  );
  deepEqual(
    watchedData[TARGETS],
    ["frame", "worker"],
    "no change made to the array"
  );

  removed = WatchedDataHelpers.removeWatchedDataEntry(watchedData, TARGETS, [
    "frame",
    "worker",
  ]);
  ok(
    removed,
    "removedWatchedDataEntry returned true as elements have been removed"
  );
  deepEqual(watchedData[TARGETS], [], "all elements were removed");
}
