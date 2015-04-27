/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/performance/recording-utils", true);

const REQUIRED_MEMORY_ACTOR_METHODS = [
  "attach", "detach", "startRecordingAllocations", "stopRecordingAllocations", "getAllocations"
];

/**
 * Constructor for a facade around an underlying ProfilerFront.
 */
function ProfilerFront (target) {
  this._target = target;
}

ProfilerFront.prototype = {
  // Connects to the targets underlying real ProfilerFront.
  connect: Task.async(function*() {
    let target = this._target;
    // Chrome and content process targets already have obtained a reference
    // to the profiler tab actor. Use it immediately.
    if (target.form && target.form.profilerActor) {
      this._profiler = target.form.profilerActor;
    }
    // Check if we already have a grip to the `listTabs` response object
    // and, if we do, use it to get to the profiler actor.
    else if (target.root && target.root.profilerActor) {
      this._profiler = target.root.profilerActor;
    }
    // Otherwise, call `listTabs`.
    else {
      this._profiler = (yield listTabs(target.client)).profilerActor;
    }

    // Fetch and store information about the SPS profiler and
    // server profiler.
    this.traits = {};
    this.traits.filterable = target.getTrait("profilerDataFilterable");
  }),

  /**
   * Makes a request to the underlying real profiler actor. Handles
   * backwards compatibility differences based off of the features
   * and traits of the actor.
   */
  _request: function (method, ...args) {
    let deferred = promise.defer();
    let data = args[0] || {};
    data.to = this._profiler;
    data.type = method;
    this._target.client.request(data, res => {
      // If the backend does not support filtering by start and endtime on platform (< Fx40),
      // do it on the client (much slower).
      if (method === "getProfile" && !this.traits.filterable) {
        RecordingUtils.filterSamples(res.profile, data.startTime || 0);
      }

      deferred.resolve(res);
    });
    return deferred.promise;
  }
};

exports.ProfilerFront = ProfilerFront;

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

/**
 * Returns a promise resolved with a listing of all the tabs in the
 * provided thread client.
 */
function listTabs(client) {
  let deferred = promise.defer();
  client.listTabs(deferred.resolve);
  return deferred.promise;
}

