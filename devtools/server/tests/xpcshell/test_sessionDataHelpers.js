/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test SessionDataHelpers.
 */

"use strict";

const { SessionDataHelpers } = ChromeUtils.import(
  "resource://devtools/server/actors/watcher/SessionDataHelpers.jsm"
);
const { SUPPORTED_DATA } = SessionDataHelpers;
const { TARGETS } = SUPPORTED_DATA;

function run_test() {
  const sessionData = {
    [TARGETS]: [],
  };

  info("Test adding a new entry");
  SessionDataHelpers.addOrSetSessionDataEntry(
    sessionData,
    TARGETS,
    ["frame", "worker"],
    "add"
  );
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker"],
    "the two elements were added"
  );

  info("Test adding a duplicated entry");
  SessionDataHelpers.addOrSetSessionDataEntry(
    sessionData,
    TARGETS,
    ["frame"],
    "add"
  );
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker"],
    "addOrSetSessionDataEntry ignore duplicates"
  );

  SessionDataHelpers.addOrSetSessionDataEntry(
    sessionData,
    TARGETS,
    ["process"],
    "add"
  );
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker", "process"],
    "the third element is added"
  );

  info("Test removing an existing entry");
  let removed = SessionDataHelpers.removeSessionDataEntry(
    sessionData,
    TARGETS,
    ["process"]
  );
  ok(removed, "removedSessionDataEntry returned true as it removed an element");
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker"],
    "the element has been remove"
  );

  info("Test removing non-existing entry");
  removed = SessionDataHelpers.removeSessionDataEntry(sessionData, TARGETS, [
    "not-existing",
  ]);
  ok(
    !removed,
    "removedSessionDataEntry returned false as no element has been removed"
  );
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker"],
    "no change made to the array"
  );

  removed = SessionDataHelpers.removeSessionDataEntry(sessionData, TARGETS, [
    "frame",
    "worker",
  ]);
  ok(
    removed,
    "removedSessionDataEntry returned true as elements have been removed"
  );
  deepEqual(sessionData[TARGETS], [], "all elements were removed");

  info("Test settting instead of adding data entries");
  SessionDataHelpers.addOrSetSessionDataEntry(
    sessionData,
    TARGETS,
    ["frame"],
    "add"
  );
  deepEqual(sessionData[TARGETS], ["frame"], "frame was re-added");

  SessionDataHelpers.addOrSetSessionDataEntry(
    sessionData,
    TARGETS,
    ["process", "worker"],
    "set"
  );
  deepEqual(
    sessionData[TARGETS],
    ["process", "worker"],
    "frame was replaced by process and worker"
  );

  info("Test setting an empty array");
  SessionDataHelpers.addOrSetSessionDataEntry(sessionData, TARGETS, [], "set");
  deepEqual(
    sessionData[TARGETS],
    [],
    "Setting an empty array of entries clears the data entry"
  );
}
