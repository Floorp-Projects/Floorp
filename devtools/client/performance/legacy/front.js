/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "extend",
  "sdk/util/object", true);

loader.lazyRequireGetter(this, "Actors",
  "devtools/client/performance/legacy/actors");
loader.lazyRequireGetter(this, "LegacyPerformanceRecording",
  "devtools/client/performance/legacy/recording", true);
loader.lazyRequireGetter(this, "importRecording",
  "devtools/client/performance/legacy/recording", true);
loader.lazyRequireGetter(this, "normalizePerformanceFeatures",
  "devtools/shared/performance/recording-utils", true);
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "getDeviceFront",
  "devtools/server/actors/device", true);
loader.lazyRequireGetter(this, "getSystemInfo",
  "devtools/shared/system", true);
loader.lazyRequireGetter(this, "events",
  "sdk/event/core");
loader.lazyRequireGetter(this, "EventTarget",
  "sdk/event/target", true);
loader.lazyRequireGetter(this, "Class",
  "sdk/core/heritage", true);

/**
 * A connection to underlying actors (profiler, framerate, etc.)
 * shared by all tools in a target.
 */
const LegacyPerformanceFront = Class({
  extends: EventTarget,

  LEGACY_FRONT: true,

  traits: {
    features: {
      withMarkers: true,
      withTicks: true,
      withMemory: false,
      withAllocations: false,
      withJITOptimizations: false,
    },
  },

  initialize: function (target) {
    let { form, client } = target;
    this._target = target;
    this._form = form;
    this._client = client;
    this._pendingConsoleRecordings = [];
    this._sitesPullTimeout = 0;
    this._recordings = [];

    this._pipeToFront = this._pipeToFront.bind(this);
    this._onTimelineData = this._onTimelineData.bind(this);
    this._onConsoleProfileStart = this._onConsoleProfileStart.bind(this);
    this._onConsoleProfileStop = this._onConsoleProfileStop.bind(this);
    this._onProfilerStatus = this._onProfilerStatus.bind(this);
    this._onProfilerUnexpectedlyStopped = this._onProfilerUnexpectedlyStopped.bind(this);
  },

  /**
   * Initializes a connection to the profiler and other miscellaneous actors.
   * If in the process of opening, or already open, nothing happens.
   *
   * @return object
   *         A promise that is resolved once the connection is established.
   */
  connect: Task.async(function*() {
    if (this._connecting) {
      return this._connecting.promise;
    }

    // Create a promise that gets resolved upon connecting, so that
    // other attempts to open the connection use the same resolution promise
    this._connecting = promise.defer();

    // Sets `this._profiler`, `this._timeline`.
    // Only initialize the timeline fronts if the respective actors
    // are available. Older Gecko versions don't have existing implementations,
    // in which case all the methods we need can be easily mocked.
    yield this._connectActors();
    yield this._registerListeners();

    this._connecting.resolve();
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
    this._client = null;
    this._form = null;
    this._target = this._target;
  }),

  /**
   * Initializes fronts and connects to the underlying actors using the facades
   * found in ./actors.js.
   */
  _connectActors: Task.async(function*() {
    this._profiler = new Actors.LegacyProfilerFront(this._target);
    this._timeline = new Actors.LegacyTimelineFront(this._target);

    yield promise.all([
      this._profiler.connect(),
      this._timeline.connect()
    ]);

    // If mocked timeline, update the traits
    this.traits.features.withMarkers = !this._timeline.IS_MOCK;
    this.traits.features.withTicks = !this._timeline.IS_MOCK;
  }),

  /**
   * Registers listeners on events from the underlying
   * actors, so the connection can handle them.
   */
  _registerListeners: function () {
    this._timeline.on("timeline-data", this._onTimelineData);
    this._profiler.on("console-profile-start", this._onConsoleProfileStart);
    this._profiler.on("console-profile-stop", this._onConsoleProfileStop);
    this._profiler.on("profiler-stopped", this._onProfilerUnexpectedlyStopped);
    this._profiler.on("profiler-status", this._onProfilerStatus);
  },

  /**
   * Unregisters listeners on events on the underlying actors.
   */
  _unregisterListeners: function () {
    this._timeline.off("timeline-data", this._onTimelineData);
    this._profiler.off("console-profile-start", this._onConsoleProfileStart);
    this._profiler.off("console-profile-stop", this._onConsoleProfileStop);
    this._profiler.off("profiler-stopped", this._onProfilerUnexpectedlyStopped);
    this._profiler.off("profiler-status", this._onProfilerStatus);
  },

  /**
   * Closes the connections to non-profiler actors.
   */
  _disconnectActors: Task.async(function* () {
    yield promise.all([
      this._profiler.destroy(),
      this._timeline.destroy(),
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

    events.emit(this, "console-profile-start");

    yield this.startRecording(extend({}, getLegacyPerformanceRecordingPrefs(), {
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
  _onConsoleProfileStop: Task.async(function *(_, data) {
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
   * - ticks
   *
   * Populate our internal store of recordings for all currently recording sessions.
   */
  _onTimelineData: function (_, ...data) {
    this._recordings.forEach(e => e._addTimelineData.apply(e, data));
    events.emit(this, "timeline-data", ...data);
  },

  /**
   * Called whenever the underlying profiler polls its current status.
   */
  _onProfilerStatus: function (_, data) {
    // If no data emitted (whether from an older actor being destroyed
    // from a previous test, or the server does not support it), just ignore.
    if (!data || data.position === void 0) {
      return;
    }

    this._currentBufferStatus = data;
    events.emit(this, "profiler-status", data);
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
    let model = new LegacyPerformanceRecording(normalizePerformanceFeatures(options, this.traits.features));

    // All actors are started asynchronously over the remote debugging protocol.
    // Get the corresponding start times from each one of them.
    // The timeline actors are target-dependent, so start those as well,
    // even though these are mocked in older Geckos (FF < 35)
    let profilerStart = this._profiler.start(options);
    let timelineStart = this._timeline.start(options);

    let { startTime, position, generation, totalSize } = yield profilerStart;
    let timelineStartTime = yield timelineStart;

    let data = {
      profilerStartTime: startTime, timelineStartTime,
      generation, position, totalSize
    };

    // Signify to the model that the recording has started,
    // populate with data and store the recording model here.
    model._populate(data);
    this._recordings.push(model);

    events.emit(this, "recording-started", model);
    return model;
  }),

  /**
   * Manually ends the recording session for the corresponding LegacyPerformanceRecording.
   *
   * @param LegacyPerformanceRecording model
   *        The corresponding LegacyPerformanceRecording that belongs to the recording session wished to stop.
   * @return LegacyPerformanceRecording
   *         Returns the same model, populated with the profiling data.
   */
  stopRecording: Task.async(function*(model) {
    // If model isn't in the LegacyPerformanceFront internal store,
    // then do nothing.
    if (this._recordings.indexOf(model) === -1) {
      return;
    }

    // Flag the recording as no longer recording, so that `model.isRecording()`
    // is false. Do this before we fetch all the data, and then subsequently
    // the recording can be considered "completed".
    let endTime = Date.now();
    model._onStoppingRecording(endTime);
    events.emit(this, "recording-stopping", model);

    // Currently there are two ways profiles stop recording. Either manually in the
    // performance tool, or via console.profileEnd. Once a recording is done,
    // we want to deliver the model to the performance tool (either as a return
    // from the LegacyPerformanceFront or via `console-profile-stop` event) and then
    // remove it from the internal store.
    //
    // In the case where a console.profile is generated via the console (so the tools are
    // open), we initialize the Performance tool so it can listen to those events.
    this._recordings.splice(this._recordings.indexOf(model), 1);

    let config = model.getConfiguration();
    let startTime = model._getProfilerStartTime();
    let profilerData = yield this._profiler.getProfile({ startTime });
    let timelineEndTime = Date.now();

    // Only if there are no more sessions recording do we stop
    // the underlying timeline actors. If we're still recording,
    // juse use Date.now() for the timeline end times, as those
    // are only used in tests.
    if (!this.isRecording()) {
      // This doesn't stop the profiler, just turns off polling for
      // events, and also turns off events on timeline actors.
      yield this._profiler.stop();
      timelineEndTime = yield this._timeline.stop(config);
    }

    let systemDeferred = promise.defer();
    this._client.listTabs(form => {
      systemDeferred.resolve(getDeviceFront(this._client, form).getDescription());
    });
    let systemHost = yield systemDeferred.promise;
    let systemClient = yield getSystemInfo();

    // Set the results on the LegacyPerformanceRecording itself.
    model._onStopRecording({
      // Data available only at the end of a recording.
      profile: profilerData.profile,

      // End times for all the actors.
      profilerEndTime: profilerData.currentTime,
      timelineEndTime: timelineEndTime,
      systemHost,
      systemClient,
    });

    events.emit(this, "recording-stopped", model);
    return model;
  }),

  /**
   * Creates a recording object when given a nsILocalFile.
   *
   * @param {nsILocalFile} file
   *        The file to import the data from.
   * @return {Promise<LegacyPerformanceRecording>}
   */
  importRecording: function (file) {
    return importRecording(file);
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
   * Pass in a PerformanceRecording and get a normalized value from 0 to 1 of how much
   * of this recording's lifetime remains without being overwritten.
   *
   * @param {PerformanceRecording} recording
   * @return {number?}
   */
  getBufferUsageForRecording: function (recording) {
    if (!recording.isRecording() || !this._currentBufferStatus) {
      return null;
    }
    let { position: currentPosition, totalSize, generation: currentGeneration } = this._currentBufferStatus;
    let { position: origPosition, generation: origGeneration } = recording.getStartingBufferStatus();

    let normalizedCurrent = (totalSize * (currentGeneration - origGeneration)) + currentPosition;
    let percent = (normalizedCurrent - origPosition) / totalSize;
    return percent > 1 ? 1 : percent;
  },

  /**
   * Returns the configurations set on underlying components, used in tests.
   * Returns an object with `probability`, `maxLogLength` for allocations, and
   * `entries` and `interval` for profiler.
   *
   * @return {object}
   */
  getConfiguration: Task.async(function *() {
    let profilerConfig = yield this._request("profiler", "getStartOptions");
    return profilerConfig;
  }),

  /**
   * An event from an underlying actor that we just want
   * to pipe to the front itself.
   */
  _pipeToFront: function (eventName, ...args) {
    events.emit(this, eventName, ...args);
  },

  /**
   * Helper method to interface with the underlying actors directly.
   * Used only in tests.
   */
  _request: function (actorName, method, ...args) {
    if (!DevToolsUtils.testing) {
      throw new Error("LegacyPerformanceFront._request may only be used in tests.");
    }
    let actor = this[`_${actorName}`];
    return actor[method].apply(actor, args);
  },

  /**
   * Sets how often the "profiler-status" event should be emitted.
   * Used in tests.
   */
  setProfilerStatusInterval: function (n) {
    if (this._profiler._poller) {
      this._profiler._poller._wait = n;
    }
    this._profiler._PROFILER_CHECK_TIMER = n;
  },

  toString: () => "[object LegacyPerformanceFront]"
});

/**
 * Creates an object of configurations based off of preferences for a LegacyPerformanceRecording.
 */
function getLegacyPerformanceRecordingPrefs () {
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

exports.LegacyPerformanceFront = LegacyPerformanceFront;
