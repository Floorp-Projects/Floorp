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

  SessionDataHelpers.addSessionDataEntry(sessionData, TARGETS, [
    "frame",
    "worker",
  ]);
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker"],
    "the two elements were added"
  );

  SessionDataHelpers.addSessionDataEntry(sessionData, TARGETS, ["frame"]);
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker"],
    "addSessionDataEntry ignore duplicates"
  );

  SessionDataHelpers.addSessionDataEntry(sessionData, TARGETS, ["process"]);
  deepEqual(
    sessionData[TARGETS],
    ["frame", "worker", "process"],
    "the third element is added"
  );

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
}
