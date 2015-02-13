/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");
const { extend } = require("sdk/util/object");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "TimelineFront",
  "devtools/server/actors/timeline", true);
loader.lazyRequireGetter(this, "MemoryFront",
  "devtools/server/actors/memory", true);
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/toolkit/DevToolsUtils");
loader.lazyRequireGetter(this, "compatibility",
  "devtools/performance/compatibility");

loader.lazyImporter(this, "gDevTools",
  "resource:///modules/devtools/gDevTools.jsm");
loader.lazyImporter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");
loader.lazyImporter(this, "clearTimeout",
  "resource://gre/modules/Timer.jsm");

// How often do we pull allocation sites from the memory actor.
const DEFAULT_ALLOCATION_SITES_PULL_TIMEOUT = 200; // ms

/**
 * A cache of all PerformanceActorsConnection instances.
 * The keys are Target objects.
 */
let SharedPerformanceActors = new WeakMap();

/**
 * Instantiates a shared PerformanceActorsConnection for the specified target.
 * Consumers must yield on `open` to make sure the connection is established.
 *
 * @param Target target
 *        The target owning this connection.
 * @return PerformanceActorsConnection
 *         The shared connection for the specified target.
 */
SharedPerformanceActors.forTarget = function(target) {
  if (this.has(target)) {
    return this.get(target);
  }

  let instance = new PerformanceActorsConnection(target);
  this.set(target, instance);
  return instance;
};

/**
 * A connection to underlying actors (profiler, memory, framerate, etc.)
 * shared by all tools in a target.
 *
 * Use `SharedPerformanceActors.forTarget` to make sure you get the same
 * instance every time, and the `PerformanceFront` to start/stop recordings.
 *
 * @param Target target
 *        The target owning this connection.
 */
function PerformanceActorsConnection(target) {
  EventEmitter.decorate(this);

  this._target = target;
  this._client = this._target.client;
  this._request = this._request.bind(this);

  Services.obs.notifyObservers(null, "performance-actors-connection-created", null);
}

PerformanceActorsConnection.prototype = {

  // Properties set when mocks are being used
  _usingMockMemory: false,
  _usingMockTimeline: false,

  /**
   * Initializes a connection to the profiler and other miscellaneous actors.
   * If in the process of opening, or already open, nothing happens.
   *
   * @return object
   *         A promise that is resolved once the connection is established.
   */
  open: Task.async(function*() {
    if (this._connected) {
      return;
    }

    // Local debugging needs to make the target remote.
    yield this._target.makeRemote();

    // Sets `this._profiler`, `this._timeline` and `this._memory`.
    // Only initialize the timeline and memory fronts if the respective actors
    // are available. Older Gecko versions don't have existing implementations,
    // in which case all the methods we need can be easily mocked.
    yield this._connectProfilerActor();
    yield this._connectTimelineActor();
    yield this._connectMemoryActor();

    this._connected = true;

    Services.obs.notifyObservers(null, "performance-actors-connection-opened", null);
  }),

  /**
   * Destroys this connection.
   */
  destroy: Task.async(function*() {
    yield this._disconnectActors();
    this._connected = false;
  }),

  /**
   * Initializes a connection to the profiler actor.
   */
  _connectProfilerActor: Task.async(function*() {
    // Chrome debugging targets have already obtained a reference
    // to the profiler actor.
    if (this._target.chrome) {
      this._profiler = this._target.form.profilerActor;
    }
    // When we are debugging content processes, we already have the tab
    // specific one. Use it immediately.
    else if (this._target.form && this._target.form.profilerActor) {
      this._profiler = this._target.form.profilerActor;
    }
    // Check if we already have a grip to the `listTabs` response object
    // and, if we do, use it to get to the profiler actor.
    else if (this._target.root && this._target.root.profilerActor) {
      this._profiler = this._target.root.profilerActor;
    }
    // Otherwise, call `listTabs`.
    else {
      this._profiler = (yield listTabs(this._client)).profilerActor;
    }
  }),

  /**
   * Initializes a connection to a timeline actor.
   */
  _connectTimelineActor: function() {
    let supported = yield compatibility.timelineActorSupported(this._target);
    if (supported) {
      this._timeline = new TimelineFront(this._target.client, this._target.form);
    } else {
      this._usingMockTimeline = true;
      this._timeline = new compatibility.MockTimelineFront();
    }
  },

  /**
   * Initializes a connection to a memory actor.
   */
  _connectMemoryActor: Task.async(function* () {
    let supported = yield compatibility.memoryActorSupported(this._target);
    if (supported) {
      this._memory = new MemoryFront(this._target.client, this._target.form);
    } else {
      this._usingMockMemory = true;
      this._memory = new compatibility.MockMemoryFront();
    }
  }),

  /**
   * Closes the connections to non-profiler actors.
   */
  _disconnectActors: Task.async(function* () {
    yield this._timeline.destroy();
    yield this._memory.destroy();
  }),

  /**
   * Sends the request over the remote debugging protocol to the
   * specified actor.
   *
   * @param string actor
   *        Currently supported: "profiler", "timeline", "memory".
   * @param string method
   *        Method to call on the backend.
   * @param any args [optional]
   *        Additional data or arguments to send with the request.
   * @return object
   *         A promise resolved with the response once the request finishes.
   */
  _request: function(actor, method, ...args) {
    // Handle requests to the profiler actor.
    if (actor == "profiler") {
      let deferred = promise.defer();
      let data = args[0] || {};
      data.to = this._profiler;
      data.type = method;
      this._client.request(data, deferred.resolve);
      return deferred.promise;
    }

    // Handle requests to the timeline actor.
    if (actor == "timeline") {
      return this._timeline[method].apply(this._timeline, args);
    }

    // Handle requests to the memory actor.
    if (actor == "memory") {
      return this._memory[method].apply(this._memory, args);
    }
  }
};

/**
 * A thin wrapper around a shared PerformanceActorsConnection for the parent target.
 * Handles manually starting and stopping a recording.
 *
 * @param PerformanceActorsConnection connection
 *        The shared instance for the parent target.
 */
function PerformanceFront(connection) {
  EventEmitter.decorate(this);

  this._request = connection._request;

  // Pipe events from TimelineActor to the PerformanceFront
  connection._timeline.on("markers", markers => this.emit("markers", markers));
  connection._timeline.on("frames", (delta, frames) => this.emit("frames", delta, frames));
  connection._timeline.on("memory", (delta, measurement) => this.emit("memory", delta, measurement));
  connection._timeline.on("ticks", (delta, timestamps) => this.emit("ticks", delta, timestamps));

  // Set when mocks are being used
  this._usingMockMemory = connection._usingMockMemory;
  this._usingMockTimeline = connection._usingMockTimeline;

  this._pullAllocationSites = this._pullAllocationSites.bind(this);
  this._sitesPullTimeout = 0;
}

PerformanceFront.prototype = {

  /**
   * Manually begins a recording session.
   *
   * @param object options
   *        An options object to pass to the actors. Supported properties are
   *        `withTicks`, `withMemory` and `withAllocations`.
   * @return object
   *         A promise that is resolved once recording has started.
   */
  startRecording: Task.async(function*(options = {}) {
    // All actors are started asynchronously over the remote debugging protocol.
    // Get the corresponding start times from each one of them.
    let profilerStartTime = yield this._startProfiler();
    let timelineStartTime = yield this._startTimeline(options);
    let memoryStartTime = yield this._startMemory(options);

    return {
      profilerStartTime,
      timelineStartTime,
      memoryStartTime
    };
  }),

  /**
   * Manually ends the current recording session.
   *
   * @param object options
   *        @see PerformanceFront.prototype.startRecording
   * @return object
   *         A promise that is resolved once recording has stopped,
   *         with the profiler and memory data, along with all the end times.
   */
  stopRecording: Task.async(function*(options = {}) {
    let memoryEndTime = yield this._stopMemory(options);
    let timelineEndTime = yield this._stopTimeline(options);
    let profilerData = yield this._request("profiler", "getProfile");

    return {
      // Data available only at the end of a recording.
      profile: profilerData.profile,

      // End times for all the actors.
      profilerEndTime: profilerData.currentTime,
      timelineEndTime: timelineEndTime,
      memoryEndTime: memoryEndTime
    };
  }),

  /**
   * Starts the profiler actor, if necessary.
   */
  _startProfiler: Task.async(function *() {
    // Start the profiler only if it wasn't already active. The built-in
    // nsIPerformance module will be kept recording, because it's the same instance
    // for all targets and interacts with the whole platform, so we don't want
    // to affect other clients by stopping (or restarting) it.
    let profilerStatus = yield this._request("profiler", "isActive");
    if (profilerStatus.isActive) {
      this.emit("profiler-already-active");
      return profilerStatus.currentTime;
    }

    // Extend the profiler options so that protocol.js doesn't modify the original.
    let profilerOptions = extend({}, this._customProfilerOptions);
    yield this._request("profiler", "startProfiler", profilerOptions);

    this.emit("profiler-activated");
    return 0;
  }),

  /**
   * Starts the timeline actor.
   */
  _startTimeline: Task.async(function *(options) {
    // The timeline actor is target-dependent, so just make sure it's recording.
    // It won't, however, be available in older Geckos (FF < 35).
    return (yield this._request("timeline", "start", options));
  }),

  /**
   * Stops the timeline actor.
   */
  _stopTimeline: Task.async(function *(options) {
    return (yield this._request("timeline", "stop"));
  }),

  /**
   * Starts the timeline actor, if necessary.
   */
  _startMemory: Task.async(function *(options) {
    if (!options.withAllocations) {
      return 0;
    }
    yield this._request("memory", "attach");
    let memoryStartTime = yield this._request("memory", "startRecordingAllocations");
    yield this._pullAllocationSites();
    return memoryStartTime;
  }),

  /**
   * Stops the timeline actor, if necessary.
   */
  _stopMemory: Task.async(function *(options) {
    if (!options.withAllocations) {
      return 0;
    }
    clearTimeout(this._sitesPullTimeout);
    let memoryEndTime = yield this._request("memory", "stopRecordingAllocations");
    yield this._request("memory", "detach");
    return memoryEndTime;
  }),

  /**
   * At regular intervals, pull allocations from the memory actor, and forward
   * them to consumers.
   */
  _pullAllocationSites: Task.async(function *() {
    let memoryData = yield this._request("memory", "getAllocations");

    this.emit("allocations", {
      sites: memoryData.allocations,
      timestamps: memoryData.allocationsTimestamps,
      frames: memoryData.frames,
      counts: memoryData.counts
    });

    let delay = DEFAULT_ALLOCATION_SITES_PULL_TIMEOUT;
    this._sitesPullTimeout = setTimeout(this._pullAllocationSites, delay);
  }),

  /**
   * Overrides the options sent to the built-in profiler module when activating,
   * such as the maximum entries count, the sampling interval etc.
   *
   * Used in tests and for older backend implementations.
   */
  _customProfilerOptions: {
    entries: 1000000,
    interval: 1,
    features: ["js"],
    threadFilters: ["GeckoMain"]
  },

  /**
   * Returns an object indicating if mock actors are being used or not.
   */
  getMocksInUse: function () {
    return {
      memory: this._usingMockMemory,
      timeline: this._usingMockTimeline
    };
  }
};

/**
 * Returns a promise resolved with a listing of all the tabs in the
 * provided thread client.
 */
function listTabs(client) {
  let deferred = promise.defer();
  client.listTabs(deferred.resolve);
  return deferred.promise;
}

exports.getPerformanceActorsConnection = target => SharedPerformanceActors.forTarget(target);
exports.PerformanceFront = PerformanceFront;
