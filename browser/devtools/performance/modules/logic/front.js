/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "extend",
  "sdk/util/object", true);

loader.lazyRequireGetter(this, "Actors",
  "devtools/performance/actors");
loader.lazyRequireGetter(this, "RecordingModel",
  "devtools/performance/recording-model", true);
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/toolkit/DevToolsUtils");

loader.lazyImporter(this, "gDevTools",
  "resource:///modules/devtools/gDevTools.jsm");

/**
 * A cache of all PerformanceFront instances.
 * The keys are Target objects.
 */
let PerformanceFronts = new WeakMap();

/**
 * Instantiates a shared PerformanceFront for the specified target.
 * Consumers must yield on `open` to make sure the connection is established.
 *
 * @param Target target
 *        The target owning this connection.
 * @return PerformanceFront
 *         The pseudofront for all the underlying actors.
 */
PerformanceFronts.forTarget = function(target) {
  if (this.has(target)) {
    return this.get(target);
  }

  let instance = new PerformanceFront(target);
  this.set(target, instance);
  return instance;
};

/**
 * A connection to underlying actors (profiler, memory, framerate, etc.)
 * shared by all tools in a target.
 *
 * Use `PerformanceFronts.forTarget` to make sure you get the same
 * instance every time, and the `PerformanceFront` to start/stop recordings.
 *
 * @param Target target
 *        The target owning this connection.
 */
function PerformanceFront (target) {
  EventEmitter.decorate(this);

  this._target = target;
  this._client = this._target.client;
  this._pendingConsoleRecordings = [];
  this._sitesPullTimeout = 0;
  this._recordings = [];

  this._pipeToFront = this._pipeToFront.bind(this);
  this._onTimelineData = this._onTimelineData.bind(this);
  this._onConsoleProfileStart = this._onConsoleProfileStart.bind(this);
  this._onConsoleProfileEnd = this._onConsoleProfileEnd.bind(this);
  this._onProfilerStatus = this._onProfilerStatus.bind(this);
  this._onProfilerUnexpectedlyStopped = this._onProfilerUnexpectedlyStopped.bind(this);

  Services.obs.notifyObservers(null, "performance-tools-connection-created", null);
}

PerformanceFront.prototype = {

  // Properties set based off of server actor support
  _memorySupported: true,
  _timelineSupported: true,

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
    this._connecting = promise.defer();

    // Local debugging needs to make the target remote.
    yield this._target.makeRemote();

    // Sets `this._profiler`, `this._timeline` and `this._memory`.
    // Only initialize the timeline and memory fronts if the respective actors
    // are available. Older Gecko versions don't have existing implementations,
    // in which case all the methods we need can be easily mocked.
    yield this._connectActors();
    yield this._registerListeners();

    this._connecting.resolve();
    Services.obs.notifyObservers(null, "performance-tools-connection-opened", null);
  }),

  /**
   * Destroys this connection.
   */
  destroy: Task.async(function*() {
    if (this._connecting) {
      yield this._connecting.promise;
    } else {
      return;
    }

    yield this._unregisterListeners();
    yield this._disconnectActors();

    this._connecting = null;
    this._profiler = null;
    this._timeline = null;
    this._memory = null;
    this._target = null;
    this._client = null;
  }),

  /**
   * Initializes fronts and connects to the underlying actors using the facades
   * found in ./actors.js.
   */
  _connectActors: Task.async(function*() {
    this._profiler = new Actors.ProfilerFront(this._target);
    this._memory = new Actors.MemoryFront(this._target);
    this._timeline = new Actors.TimelineFront(this._target);

    yield promise.all([
      this._profiler.connect(),
      this._memory.connect(),
      this._timeline.connect()
    ]);

    // Expose server support status of underlying actors
    // after connecting.
    this._memorySupported = !this._memory.IS_MOCK;
    this._timelineSupported = !this._timeline.IS_MOCK;
  }),

  /**
   * Registers listeners on events from the underlying
   * actors, so the connection can handle them.
   */
  _registerListeners: function () {
    this._timeline.on("timeline-data", this._onTimelineData);
    this._memory.on("timeline-data", this._onTimelineData);
    this._profiler.on("console-profile-start", this._onConsoleProfileStart);
    this._profiler.on("console-profile-end", this._onConsoleProfileEnd);
    this._profiler.on("profiler-stopped", this._onProfilerUnexpectedlyStopped);
    this._profiler.on("profiler-already-active", this._pipeToFront);
    this._profiler.on("profiler-activated", this._pipeToFront);
    this._profiler.on("profiler-status", this._onProfilerStatus);
  },

  /**
   * Unregisters listeners on events on the underlying actors.
   */
  _unregisterListeners: function () {
    this._timeline.off("timeline-data", this._onTimelineData);
    this._memory.off("timeline-data", this._onTimelineData);
    this._profiler.off("console-profile-start", this._onConsoleProfileStart);
    this._profiler.off("console-profile-end", this._onConsoleProfileEnd);
    this._profiler.off("profiler-stopped", this._onProfilerUnexpectedlyStopped);
    this._profiler.off("profiler-already-active", this._pipeToFront);
    this._profiler.off("profiler-activated", this._pipeToFront);
    this._profiler.off("profiler-status", this._onProfilerStatus);
  },

  /**
   * Closes the connections to non-profiler actors.
   */
  _disconnectActors: Task.async(function* () {
    yield promise.all([
      this._profiler.destroy(),
      this._timeline.destroy(),
      this._memory.destroy()
    ]);
  }),

  /**
   * Invoked whenever `console.profile` is called.
   *
   * @param string profileLabel
   *        The provided string argument if available; undefined otherwise.
   * @param number currentTime
   *        The time (in milliseconds) when the call was made, relative to when
   *        the nsIProfiler module was started.
   */
  _onConsoleProfileStart: Task.async(function *(_, { profileLabel, currentTime: startTime }) {
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
  }),

  /**
   * Invoked whenever `console.profileEnd` is called.
   *
   * @param string profileLabel
   *        The provided string argument if available; undefined otherwise.
   * @param number currentTime
   *        The time (in milliseconds) when the call was made, relative to when
   *        the nsIProfiler module was started.
   */
  _onConsoleProfileEnd: Task.async(function *(_, data) {
    // If no data, abort; can occur if profiler isn't running and we get a surprise
    // call to console.profileEnd()
    if (!data) {
      return;
    }
    let { profileLabel, currentTime: endTime } = data;

    let pending = this._recordings.filter(r => r.isConsole() && r.isRecording());
    if (pending.length === 0) {
      return;
    }

    let model;
    // Try to find the corresponding `console.profile` call if
    // a label was used in profileEnd(). If no matches, abort.
    if (profileLabel) {
      model = pending.find(e => e.getLabel() === profileLabel);
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

    yield this.stopRecording(model);
  }),

 /**
  * TODO handle bug 1144438
  */
  _onProfilerUnexpectedlyStopped: function () {
    Cu.reportError("Profiler unexpectedly stopped.", arguments);
  },

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
  _onTimelineData: function (_, ...data) {
    this._recordings.forEach(e => e._addTimelineData.apply(e, data));
    this.emit("timeline-data", ...data);
  },

  /**
   * Called whenever the underlying profiler polls its current status.
   */
  _onProfilerStatus: function (_, data) {
    // If no data emitted (whether from an older actor being destroyed
    // from a previous test, or the server does not support it), just ignore.
    if (!data) {
      return;
    }
    // Check for a value of buffer status (`position`) to see if the server
    // supports buffer status -- apply to the recording models if so.
    if (data.position !== void 0) {
      this._recordings.forEach(e => e._addBufferStatusData.call(e, data));
    }
    this.emit("profiler-status", data);
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
    this.emit("recording-starting", model);

    // All actors are started asynchronously over the remote debugging protocol.
    // Get the corresponding start times from each one of them.
    // The timeline and memory actors are target-dependent, so start those as well,
    // even though these are mocked in older Geckos (FF < 35)
    let { startTime, position, generation, totalSize } = yield this._profiler.start(options);
    let timelineStartTime = yield this._timeline.start(options);
    let memoryStartTime = yield this._memory.start(options);

    let data = {
      profilerStartTime: startTime, timelineStartTime, memoryStartTime,
      generation, position, totalSize
    };

    // Signify to the model that the recording has started,
    // populate with data and store the recording model here.
    model._populate(data);
    this._recordings.push(model);

    this.emit("recording-started", model);
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
    // If model isn't in the PerformanceFront internal store,
    // then do nothing.
    if (this._recordings.indexOf(model) === -1) {
      return;
    }

    // Flag the recording as no longer recording, so that `model.isRecording()`
    // is false. Do this before we fetch all the data, and then subsequently
    // the recording can be considered "completed".
    let endTime = Date.now();
    model._onStoppingRecording(endTime);
    this.emit("recording-stopping", model);

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
    let startTime = model.getProfilerStartTime();
    let profilerData = yield this._profiler.getProfile({ startTime });
    let memoryEndTime = Date.now();
    let timelineEndTime = Date.now();

    // Only if there are no more sessions recording do we stop
    // the underlying memory and timeline actors. If we're still recording,
    // juse use Date.now() for the memory and timeline end times, as those
    // are only used in tests.
    if (!this.isRecording()) {
      // This doesn't stop the profiler, just turns off polling for
      // events, and also turns off events on memory/timeline actors.
      yield this._profiler.stop();
      memoryEndTime = yield this._memory.stop(config);
      timelineEndTime = yield this._timeline.stop(config);
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

    this.emit("recording-stopped", model);
    return model;
  }),

  /**
   * Returns an object indicating what server actors are available and
   * initialized. A falsy value indicates that the server does not support
   * that feature, or that mock actors were explicitly requested (tests).
   */
  getActorSupport: function () {
    return {
      memory: this._memorySupported,
      timeline: this._timelineSupported
    };
  },

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
   * An event from an underlying actor that we just want
   * to pipe to the front itself.
   */
  _pipeToFront: function (eventName, ...args) {
    this.emit(eventName, ...args);
  },

  /**
   * Helper method to interface with the underlying actors directly.
   * Used only in tests.
   */
  _request: function (actorName, method, ...args) {
    if (!DevToolsUtils.testing) {
      throw new Error("PerformanceFront._request may only be used in tests.");
    }
    let actor = this[`_${actorName}`];
    return actor[method].apply(actor, args);
  },

  toString: () => "[object PerformanceFront]"
};

/**
 * Creates an object of configurations based off of preferences for a RecordingModel.
 */
function getRecordingModelPrefs () {
  return {
    withMarkers: true,
    withMemory: Services.prefs.getBoolPref("devtools.performance.ui.enable-memory"),
    withTicks: Services.prefs.getBoolPref("devtools.performance.ui.enable-framerate"),
    withAllocations: Services.prefs.getBoolPref("devtools.performance.ui.enable-allocations"),
    withJITOptimizations: Services.prefs.getBoolPref("devtools.performance.ui.enable-jit-optimizations"),
    allocationsSampleProbability: +Services.prefs.getCharPref("devtools.performance.memory.sample-probability"),
    allocationsMaxLogLength: Services.prefs.getIntPref("devtools.performance.memory.max-log-length")
  };
}

exports.getPerformanceFront = t => PerformanceFronts.forTarget(t);
exports.PerformanceFront = PerformanceFront;
