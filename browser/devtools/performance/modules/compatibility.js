/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

const REQUIRED_MEMORY_ACTOR_METHODS = [
  "attach", "detach", "startRecordingAllocations", "stopRecordingAllocations", "getAllocations"
];

/**
 * A dummy front decorated with the provided methods.
 *
 * @param array blueprint
 *        A list of [funcName, retVal] describing the class.
 */
function MockFront (blueprint) {
  EventEmitter.decorate(this);

  for (let [funcName, retVal] of blueprint) {
    this[funcName] = (x => typeof x === "function" ? x() : x).bind(this, retVal);
  }
}

function MockMemoryFront () {
  MockFront.call(this, [
    ["initialize"],
    ["destroy"],
    ["attach"],
    ["detach"],
    ["getState", "detached"],
    ["startRecordingAllocations", 0],
    ["stopRecordingAllocations", 0],
    ["getAllocations", createMockAllocations],
  ]);
}
exports.MockMemoryFront = MockMemoryFront;

function MockTimelineFront () {
  MockFront.call(this, [
    ["initialize"],
    ["destroy"],
    ["start", 0],
    ["stop", 0],
  ]);
}
exports.MockTimelineFront = MockTimelineFront;

/**
 * Create a fake allocations object, to be used with the MockMemoryFront
 * so we create a fresh object each time.
 *
 * @return {Object}
 */
function createMockAllocations () {
  return {
    allocations: [],
    allocationsTimestamps: [],
    frames: [],
    counts: []
  };
}

/**
 * Takes a TabTarget, and checks through all methods that are needed
 * on the server's memory actor to determine if a mock or real MemoryActor
 * should be used. The last of the methods added to MemoryActor
 * landed in Gecko 35, so previous versions will fail this. Setting the `target`'s
 * TEST_MOCK_MEMORY_ACTOR property to true will cause this function to indicate that
 * the memory actor is not supported.
 *
 * @param {TabTarget} target
 * @return {Boolean}
 */
function memoryActorSupported (target) {
  // This `target` property is used only in tests to test
  // instances where the memory actor is not available.
  if (target.TEST_MOCK_MEMORY_ACTOR) {
    return false;
  }

  // We need to test that both the root actor has `memoryActorAllocations`,
  // which is in Gecko 38+, and also that the target has a memory actor. There
  // are scenarios, like addon debugging, where memoryActorAllocations is available,
  // but no memory actor (like addon debugging in Gecko 38+)
  return !!target.getTrait("memoryActorAllocations") && target.hasActor("memory");
}
exports.memoryActorSupported = Task.async(memoryActorSupported);

/**
 * Takes a TabTarget, and checks existence of a TimelineActor on
 * the server, or if TEST_MOCK_TIMELINE_ACTOR is to be used.
 *
 * @param {TabTarget} target
 * @return {Boolean}
 */
function timelineActorSupported(target) {
  // This `target` property is used only in tests to test
  // instances where the timeline actor is not available.
  if (target.TEST_MOCK_TIMELINE_ACTOR) {
    return false;
  }

  return target.hasActor("timeline");
}
exports.timelineActorSupported = Task.async(timelineActorSupported);
