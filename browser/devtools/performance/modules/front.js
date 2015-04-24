/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");
const { extend } = require("sdk/util/object");
const { RecordingModel } = require("devtools/performance/recording-model");

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
loader.lazyImporter(this, "Promise",
  "resource://gre/modules/Promise.jsm");


// How often do we pull allocation sites from the memory actor.
const DEFAULT_ALLOCATION_SITES_PULL_TIMEOUT = 200; // ms

// Events to pipe from PerformanceActorsConnection to the PerformanceFront
const CONNECTION_PIPE_EVENTS = [
  "console-profile-start", "console-profile-ending", "console-profile-end",
  "timeline-data", "profiler-already-active", "profiler-activated"
];

// Events to listen to from the profiler actor
const PROFILER_EVENTS = ["console-api-profiler", "profiler-stopped"];

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
  this._pendingConsoleRecordings = [];
  this._sitesPullTimeout = 0;
  this._recordings = [];

  this._onTimelineMarkers = this._onTimelineMarkers.bind(this);
  this._onTimelineFrames = this._onTimelineFrames.bind(this);
  this._onTimelineMemory = this._onTimelineMemory.bind(this);
  this._onTimelineTicks = this._onTimelineTicks.bind(this);
  this._onProfilerEvent = this._onProfilerEvent.bind(this);
  this._pullAllocationSites = this._pullAllocationSites.bind(this);

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
    if (this._connecting) {
      return this._connecting.promise;
    }

    // Create a promise that gets resolved upon connecting, so that
    // other attempts to open the connection use the same resolution promise
    this._connecting = Promise.defer();

    // Local debugging needs to make the target remote.
    yield this._target.makeRemote();

    // Sets `this._profiler`, `this._timeline` and `this._memory`.
    // Only initialize the timeline and memory fronts if the respective actors
    // are available. Older Gecko versions don't have existing implementations,
    // in which case all the methods we need can be easily mocked.
    yield this._connectProfilerActor();
    yield this._connectTimelineActor();
    yield this._connectMemoryActor();

    yield this._registerListeners();

    this._connected = true;

    this._connecting.resolve();
    Services.obs.notifyObservers(null, "performance-actors-connection-opened", null);
  }),

  /**
   * Destroys this connection.
   */
  destroy: Task.async(function*() {
    if (this._connecting && !this._connected) {
      console.warn("Attempting to destroy SharedPerformanceActorsConnection before initialization completion. If testing, ensure `gDevTools.testing` is set.");
    }

    yield this._unregisterListeners();
    yield this._disconnectActors();

    this._memory = this._timeline = this._profiler = this._target = this._client = null;
    this._connected = false;
  }),

  /**
   * Initializes a connection to the profiler actor.
   */
  _connectProfilerActor: Task.async(function*() {
    // Chrome and content process targets already have obtained a reference
    // to the profiler tab actor. Use it immediately.
    if (this._target.form && this._target.form.profilerActor) {
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
   * Registers listeners on events from the underlying
   * actors, so the connection can handle them.
   */
  _registerListeners: Task.async(function*() {
    // Pipe events from TimelineActor to the PerformanceFront
    this._timeline.on("markers", this._onTimelineMarkers);
    this._timeline.on("frames", this._onTimelineFrames);
    this._timeline.on("memory", this._onTimelineMemory);
    this._timeline.on("ticks", this._onTimelineTicks);

    // Register events on the profiler actor to hook into `console.profile*` calls.
    yield this._request("profiler", "registerEventNotifications", { events: PROFILER_EVENTS });
    this._client.addListener("eventNotification", this._onProfilerEvent);
  }),

  /**
   * Unregisters listeners on events on the underlying actors.
   */
  _unregisterListeners: Task.async(function*() {
    this._timeline.off("markers", this._onTimelineMarkers);
    this._timeline.off("frames", this._onTimelineFrames);
    this._timeline.off("memory", this._onTimelineMemory);
    this._timeline.off("ticks", this._onTimelineTicks);

    yield this._request("profiler", "unregisterEventNotifications", { events: PROFILER_EVENTS });
    this._client.removeListener("eventNotification", this._onProfilerEvent);
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
  },

  /**
   * Invoked whenever a registered event was emitted by the profiler actor.
   *
   * @param object response
   *        The data received from the backend.
   */
  _onProfilerEvent: function (_, { topic, subject, details }) {
    if (topic === "console-api-profiler") {
      if (subject.action === "profile") {
        this._onConsoleProfileStart(details);
      } else if (subject.action === "profileEnd") {
        this._onConsoleProfileEnd(details);
      }
    } else if (topic === "profiler-stopped") {
      this._onProfilerUnexpectedlyStopped();
    }
  },

  /**
   * TODO handle bug 1144438
   */
  _onProfilerUnexpectedlyStopped: function () {

  },

  /**
   * Invoked whenever `console.profile` is called.
   *
   * @param string profileLabel
   *        The provided string argument if available; undefined otherwise.
   * @param number currentTime
   *        The time (in milliseconds) when the call was made, relative to when
   *        the nsIProfiler module was started.
   */
  _onConsoleProfileStart: Task.async(function *({ profileLabel, currentTime: startTime }) {
    let recordings = this._recordings;

    // Abort if a profile with this label already exists.
    if (recordings.find(e => e.getLabel() === profileLabel)) {
      return;
    }

    // Ensure the performance front is set up and ready.
    // Slight performance overhead for this, should research some more.
    // This is to ensure that there is a front to receive the events for
    // the console profiles.
    yield gDevTools.getToolbox(this._target).loadTool("performance");

    let model = yield this.startRecording(extend(getRecordingModelPrefs(), {
      console: true,
      label: profileLabel
    }));

    this.emit("console-profile-start", model);
  }),

  /**
   * Invoked whenever `console.profileEnd` is called.
   *
   * @param object profilerData
   *        The dump of data from the profiler triggered by this console.profileEnd call.
   */
  _onConsoleProfileEnd: Task.async(function *(profilerData) {
    let pending = this._recordings.filter(r => r.isConsole() && r.isRecording());
    if (pending.length === 0) {
      return;
    }

    let model;
    // Try to find the corresponding `console.profile` call if
    // a label was used in profileEnd(). If no matches, abort.
    if (profilerData.profileLabel) {
      model = pending.find(e => e.getLabel() === profilerData.profileLabel);
    }
    // If no label supplied, pop off the most recent pending console recording
    else {
      model = pending[pending.length - 1];
    }

    // If `profileEnd()` was called with a label, and there are no matching
    // sessions, abort.
    if (!model) {
      Cu.reportError("console.profileEnd() called with label that does not match a recording.");
      return;
    }

    this.emit("console-profile-ending", model);
    yield this.stopRecording(model);
    this.emit("console-profile-end", model);
  }),

  /**
   * Handlers for TimelineActor events. All pipe to `_onTimelineData`
   * with the appropriate event name.
   */
  _onTimelineMarkers: function (markers) { this._onTimelineData("markers", markers); },
  _onTimelineFrames: function (delta, frames) { this._onTimelineData("frames", delta, frames); },
  _onTimelineMemory: function (delta, measurement) { this._onTimelineData("memory", delta, measurement); },
  _onTimelineTicks: function (delta, timestamps) { this._onTimelineData("ticks", delta, timestamps); },

  /**
   * Called whenever there is timeline data of any of the following types:
   * - markers
   * - frames
   * - memory
   * - ticks
   * - allocations
   *
   * Populate our internal store of recordings for all currently recording sessions.
   */

  _onTimelineData: function (...data) {
    this._recordings.forEach(e => e.addTimelineData.apply(e, data));
    this.emit("timeline-data", ...data);
  },

  /**
   * Begins a recording session
   *
   * @param object options
   *        An options object to pass to the actors. Supported properties are
   *        `withTicks`, `withMemory` and `withAllocations`, `probability`, and `maxLogLength`.
   * @return object
   *         A promise that is resolved once recording has started.
   */
  startRecording: Task.async(function*(options = {}) {
    let model = new RecordingModel(options);
    // All actors are started asynchronously over the remote debugging protocol.
    // Get the corresponding start times from each one of them.
    let profilerStartTime = yield this._startProfiler();
    let timelineStartTime = yield this._startTimeline(options);
    let memoryStartTime = yield this._startMemory(options);

    let data = {
      profilerStartTime,
      timelineStartTime,
      memoryStartTime
    };

    // Signify to the model that the recording has started,
    // populate with data and store the recording model here.
    model.populate(data);
    this._recordings.push(model);

    return model;
  }),

  /**
   * Manually ends the recording session for the corresponding RecordingModel.
   *
   * @param RecordingModel model
   *        The corresponding RecordingModel that belongs to the recording session wished to stop.
   * @return RecordingModel
   *         Returns the same model, populated with the profiling data.
   */
  stopRecording: Task.async(function*(model) {
    // If model isn't in the PerformanceActorsConnections internal store,
    // then do nothing.
    if (this._recordings.indexOf(model) === -1) {
      return;
    }

    // Currently there are two ways profiles stop recording. Either manually in the
    // performance tool, or via console.profileEnd. Once a recording is done,
    // we want to deliver the model to the performance tool (either as a return
    // from the PerformanceFront or via `console-profile-end` event) and then
    // remove it from the internal store.
    //
    // In the case where a console.profile is generated via the console (so the tools are
    // open), we initialize the Performance tool so it can listen to those events.
    this._recordings.splice(this._recordings.indexOf(model), 1);

    let config = model.getConfiguration();
    let profilerData = yield this._request("profiler", "getProfile");
    let memoryEndTime = Date.now();
    let timelineEndTime = Date.now();

    // Only if there are no more sessions recording do we stop
    // the underlying memory and timeline actors. If we're still recording,
    // juse use Date.now() for the memory and timeline end times, as those
    // are only used in tests.
    if (!this.isRecording()) {
      memoryEndTime = yield this._stopMemory(config);
      timelineEndTime = yield this._stopTimeline(config);
    }

    // Set the results on the RecordingModel itself.
    model._onStopRecording({
      // Data available only at the end of a recording.
      profile: profilerData.profile,

      // End times for all the actors.
      profilerEndTime: profilerData.currentTime,
      timelineEndTime: timelineEndTime,
      memoryEndTime: memoryEndTime
    });

    return model;
  }),

  /**
   * Checks all currently stored recording models and returns a boolean
   * if there is a session currently being recorded.
   *
   * @return Boolean
   */
  isRecording: function () {
    return this._recordings.some(recording => recording.isRecording());
  },

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

    // If this._customProfilerOptions is defined, use those to pass in
    // to the profiler actor. The profiler actor handles all the defaults
    // now, so this should only be used for tests.
    let profilerOptions = this._customProfilerOptions || {};
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
   * Starts polling for allocations from the memory actor, if necessary.
   */
  _startMemory: Task.async(function *(options) {
    if (!options.withAllocations) {
      return 0;
    }
    let memoryStartTime = yield this._startRecordingAllocations(options);
    yield this._pullAllocationSites();
    return memoryStartTime;
  }),

  /**
   * Stops polling for allocations from the memory actor, if necessary.
   */
  _stopMemory: Task.async(function *(options) {
    if (!options.withAllocations) {
      return 0;
    }
    // Since `_pullAllocationSites` is usually running inside a timeout, and
    // it's performing asynchronous requests to the server, a recording may
    // be stopped before that method finishes executing. Therefore, we need to
    // wait for the last request to `getAllocations` to finish before actually
    // stopping recording allocations.
    yield this._lastPullAllocationSitesFinished;
    clearTimeout(this._sitesPullTimeout);

    return yield this._stopRecordingAllocations();
  }),

  /**
   * Starts recording allocations in the memory actor.
   */
  _startRecordingAllocations: Task.async(function*(options) {
    yield this._request("memory", "attach");
    let memoryStartTime = yield this._request("memory", "startRecordingAllocations", {
      probability: options.allocationsSampleProbability,
      maxLogLength: options.allocationsMaxLogLength
    });
    return memoryStartTime;
  }),

  /**
   * Stops recording allocations in the memory actor.
   */
  _stopRecordingAllocations: Task.async(function*() {
    let memoryEndTime = yield this._request("memory", "stopRecordingAllocations");
    yield this._request("memory", "detach");
    return memoryEndTime;
  }),

  /**
   * At regular intervals, pull allocations from the memory actor, and forward
   * them to consumers.
   */
  _pullAllocationSites: Task.async(function *() {
    let deferred = promise.defer();
    this._lastPullAllocationSitesFinished = deferred.promise;

    let isDetached = (yield this._request("memory", "getState")) !== "attached";
    if (isDetached) {
      deferred.resolve();
      return;
    }

    let memoryData = yield this._request("memory", "getAllocations");

    this._onTimelineData("allocations", {
      sites: memoryData.allocations,
      timestamps: memoryData.allocationsTimestamps,
      frames: memoryData.frames,
      counts: memoryData.counts
    });

    let delay = DEFAULT_ALLOCATION_SITES_PULL_TIMEOUT;
    this._sitesPullTimeout = setTimeout(this._pullAllocationSites, delay);

    deferred.resolve();
  }),

  toString: () => "[object PerformanceActorsConnection]"
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

  this._connection = connection;
  this._request = connection._request;

  // Set when mocks are being used
  this._usingMockMemory = connection._usingMockMemory;
  this._usingMockTimeline = connection._usingMockTimeline;

  // Pipe the console profile events from the connection
  // to the front so that the UI can listen.
  CONNECTION_PIPE_EVENTS.forEach(eventName => this._connection.on(eventName, () => this.emit.apply(this, arguments)));
}

PerformanceFront.prototype = {

  /**
   * Manually begins a recording session and creates a RecordingModel.
   * Calls the underlying PerformanceActorsConnection's startRecording method.
   *
   * @param object options
   *        An options object to pass to the actors. Supported properties are
   *        `withTicks`, `withMemory` and `withAllocations`, `probability` and `maxLogLength`.
   * @return object
   *         A promise that is resolved once recording has started.
   */
  startRecording: function (options) {
    return this._connection.startRecording(options);
  },

  /**
   * Manually ends the recording session for the corresponding RecordingModel.
   * Calls the underlying PerformanceActorsConnection's
   *
   * @param RecordingModel model
   *        The corresponding RecordingModel that belongs to the recording session wished to stop.
   * @return RecordingModel
   *         Returns the same model, populated with the profiling data.
   */
  stopRecording: function (model) {
    return this._connection.stopRecording(model);
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

/**
 * Creates an object of configurations based off of preferences for a RecordingModel.
 */
function getRecordingModelPrefs () {
  return {
    withMemory: Services.prefs.getBoolPref("devtools.performance.ui.enable-memory"),
    withTicks: Services.prefs.getBoolPref("devtools.performance.ui.enable-framerate"),
    withAllocations: Services.prefs.getBoolPref("devtools.performance.ui.enable-memory"),
    allocationsSampleProbability: +Services.prefs.getCharPref("devtools.performance.memory.sample-probability"),
    allocationsMaxLogLength: Services.prefs.getIntPref("devtools.performance.memory.max-log-length")
  };
}

exports.getPerformanceActorsConnection = target => SharedPerformanceActors.forTarget(target);
exports.PerformanceFront = PerformanceFront;
