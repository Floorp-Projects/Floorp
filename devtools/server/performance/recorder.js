/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "extend",
  "sdk/util/object", true);
loader.lazyRequireGetter(this, "Class",
  "sdk/core/heritage", true);
loader.lazyRequireGetter(this, "EventTarget",
  "sdk/event/target", true);
loader.lazyRequireGetter(this, "events",
  "sdk/event/core");

loader.lazyRequireGetter(this, "Memory",
  "devtools/server/performance/memory", true);
loader.lazyRequireGetter(this, "Timeline",
  "devtools/server/performance/timeline", true);
loader.lazyRequireGetter(this, "Profiler",
  "devtools/server/performance/profiler", true);
loader.lazyRequireGetter(this, "PerformanceRecordingActor",
  "devtools/server/actors/performance-recording", true);
loader.lazyRequireGetter(this, "PerformanceRecordingFront",
  "devtools/server/actors/performance-recording", true);
loader.lazyRequireGetter(this, "mapRecordingOptions",
  "devtools/shared/performance/recording-utils", true);
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "getSystemInfo",
  "devtools/shared/system", true);

const PROFILER_EVENTS = [
  "console-api-profiler",
  "profiler-started",
  "profiler-stopped",
  "profiler-status"
];

// Max time in milliseconds for the allocations event to occur, which will
// occur on every GC, or at least as often as DRAIN_ALLOCATIONS_TIMEOUT.
const DRAIN_ALLOCATIONS_TIMEOUT = 2000;

/**
 * A connection to underlying actors (profiler, memory, framerate, etc.)
 * shared by all tools in a target.
 *
 * @param Target target
 *        The target owning this connection.
 */
const PerformanceRecorder = exports.PerformanceRecorder = Class({
  extends: EventTarget,

  initialize: function (conn, tabActor) {
    this.conn = conn;
    this.tabActor = tabActor;

    this._pendingConsoleRecordings = [];
    this._recordings = [];

    this._onTimelineData = this._onTimelineData.bind(this);
    this._onProfilerEvent = this._onProfilerEvent.bind(this);
  },

  /**
   * Initializes a connection to the profiler and other miscellaneous actors.
   * If in the process of opening, or already open, nothing happens.
   *
   * @param {Object} options.systemClient
   *        Metadata about the client's system to attach to the recording models.
   *
   * @return object
   *         A promise that is resolved once the connection is established.
   */
  connect: function (options) {
    if (this._connected) {
      return;
    }

    // Sets `this._profiler`, `this._timeline` and `this._memory`.
    // Only initialize the timeline and memory fronts if the respective actors
    // are available. Older Gecko versions don't have existing implementations,
    // in which case all the methods we need can be easily mocked.
    this._connectComponents();
    this._registerListeners();

    this._systemClient = options.systemClient;

    this._connected = true;
  },

  /**
   * Destroys this connection.
   */
  destroy: function () {
    this._unregisterListeners();
    this._disconnectComponents();

    this._connected = null;
    this._profiler = null;
    this._timeline = null;
    this._memory = null;
    this._target = null;
    this._client = null;
  },

  /**
   * Initializes fronts and connects to the underlying actors using the facades
   * found in ./actors.js.
   */
  _connectComponents: function () {
    this._profiler = new Profiler(this.tabActor);
    this._memory = new Memory(this.tabActor);
    this._timeline = new Timeline(this.tabActor);
    this._profiler.registerEventNotifications({ events: PROFILER_EVENTS });
  },

  /**
   * Registers listeners on events from the underlying
   * actors, so the connection can handle them.
   */
  _registerListeners: function () {
    this._timeline.on("*", this._onTimelineData);
    this._memory.on("*", this._onTimelineData);
    this._profiler.on("*", this._onProfilerEvent);
  },

  /**
   * Unregisters listeners on events on the underlying actors.
   */
  _unregisterListeners: function () {
    this._timeline.off("*", this._onTimelineData);
    this._memory.off("*", this._onTimelineData);
    this._profiler.off("*", this._onProfilerEvent);
  },

  /**
   * Closes the connections to non-profiler actors.
   */
  _disconnectComponents: function () {
    this._profiler.unregisterEventNotifications({ events: PROFILER_EVENTS });
    this._profiler.destroy();
    this._timeline.destroy();
    this._memory.destroy();
  },

  _onProfilerEvent: function (topic, data) {
    if (topic === "console-api-profiler") {
      if (data.subject.action === "profile") {
        this._onConsoleProfileStart(data.details);
      } else if (data.subject.action === "profileEnd") {
        this._onConsoleProfileEnd(data.details);
      }
    } else if (topic === "profiler-stopped") {
      this._onProfilerUnexpectedlyStopped();
    } else if (topic === "profiler-status") {
      events.emit(this, "profiler-status", data);
    }
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

    // Immediately emit this so the client can start setting things up,
    // expecting a recording very soon.
    events.emit(this, "console-profile-start");

    let model = yield this.startRecording(extend({}, getPerformanceRecordingPrefs(), {
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
  _onConsoleProfileEnd: Task.async(function *(data) {
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
   */
  _onTimelineData: function (eventName, ...data) {
    let eventData = Object.create(null);

    switch (eventName) {
      case "markers": {
        eventData = { markers: data[0], endTime: data[1] };
        break;
      }
      case "ticks": {
        eventData = { delta: data[0], timestamps: data[1] };
        break;
      }
      case "memory": {
        eventData = { delta: data[0], measurement: data[1] };
        break;
      }
      case "frames": {
        eventData = { delta: data[0], frames: data[1] };
        break;
      }
      case "allocations": {
        eventData = data[0];
        break;
      }
    }

    // Filter by only recordings that are currently recording;
    // TODO should filter by recordings that have realtimeMarkers enabled.
    let activeRecordings = this._recordings.filter(r => r.isRecording());

    if (activeRecordings.length) {
      events.emit(this, "timeline-data", eventName, eventData, activeRecordings);
    }
  },

  /**
   * Checks whether or not recording is currently supported. At the moment,
   * this is only influenced by private browsing mode and the profiler.
   */
  canCurrentlyRecord: function() {
    let success = true;
    let reasons = [];

    if (!Profiler.canProfile()) {
      success = false,
      reasons.push("profiler-unavailable");
    }

    // Check other factors that will affect the possibility of successfully
    // starting a recording here.

    return { success, reasons };
  },

  /**
   * Begins a recording session
   *
   * @param boolean options.withMarkers
   * @param boolean options.withTicks
   * @param boolean options.withMemory
   * @param boolean options.withAllocations
   * @param boolean options.allocationsSampleProbability
   * @param boolean options.allocationsMaxLogLength
   * @param boolean options.bufferSize
   * @param boolean options.sampleFrequency
   * @param boolean options.console
   * @param string options.label
   * @param boolean options.realtimeMarkers
   * @return object
   *         A promise that is resolved once recording has started.
   */
  startRecording: Task.async(function*(options) {
    let profilerStart, timelineStart, memoryStart;

    profilerStart = Task.spawn(function *() {
      let data = yield this._profiler.isActive();
      if (data.isActive) {
        return data;
      }
      let startData = yield this._profiler.start(mapRecordingOptions("profiler", options));

      // If no current time is exposed from starting, set it to 0 -- this is an
      // older Gecko that does not return its starting time, and uses an epoch based
      // on the profiler's start time.
      if (startData.currentTime == null) {
        startData.currentTime = 0;
      }
      return startData;
    }.bind(this));

    // Timeline will almost always be on if using the DevTools, but using component
    // independently could result in no timeline.
    if (options.withMarkers || options.withTicks || options.withMemory) {
      timelineStart = this._timeline.start(mapRecordingOptions("timeline", options));
    }

    if (options.withAllocations) {
      if (this._memory.getState() === "detached") {
        this._memory.attach();
      }
      memoryStart = this._memory.startRecordingAllocations(extend(mapRecordingOptions("memory", options), {
        drainAllocationsTimeout: DRAIN_ALLOCATIONS_TIMEOUT
      }));
    }

    let [profilerStartData, timelineStartData, memoryStartData] = yield promise.all([
      profilerStart, timelineStart, memoryStart
    ]);

    let data = Object.create(null);
    // Filter out start times that are not actually used (0 or undefined), and
    // find the earliest time since all sources use same epoch.
    let startTimes = [profilerStartData.currentTime, memoryStartData, timelineStartData].filter(Boolean);
    data.startTime = Math.min(...startTimes);
    data.position = profilerStartData.position;
    data.generation = profilerStartData.generation;
    data.totalSize = profilerStartData.totalSize;

    data.systemClient = this._systemClient;
    data.systemHost = yield getSystemInfo();

    let model = new PerformanceRecordingActor(this.conn, options, data);
    this._recordings.push(model);

    events.emit(this, "recording-started", model);
    return model;
  }),

  /**
   * Manually ends the recording session for the corresponding PerformanceRecording.
   *
   * @param PerformanceRecording model
   *        The corresponding PerformanceRecording that belongs to the recording session wished to stop.
   * @return PerformanceRecording
   *         Returns the same model, populated with the profiling data.
   */
  stopRecording: Task.async(function *(model) {
    // If model isn't in the Recorder's internal store,
    // then do nothing, like if this was a console.profileEnd
    // from a different target.
    if (this._recordings.indexOf(model) === -1) {
      return model;
    }

    // Flag the recording as no longer recording, so that `model.isRecording()`
    // is false. Do this before we fetch all the data, and then subsequently
    // the recording can be considered "completed".
    let endTime = Date.now();
    events.emit(this, "recording-stopping", model);

    // Currently there are two ways profiles stop recording. Either manually in the
    // performance tool, or via console.profileEnd. Once a recording is done,
    // we want to deliver the model to the performance tool (either as a return
    // from the PerformanceFront or via `console-profile-stop` event) and then
    // remove it from the internal store.
    //
    // In the case where a console.profile is generated via the console (so the tools are
    // open), we initialize the Performance tool so it can listen to those events.
    this._recordings.splice(this._recordings.indexOf(model), 1);

    let startTime = model._startTime;
    let profilerData = this._profiler.getProfile({ startTime });

    // Only if there are no more sessions recording do we stop
    // the underlying memory and timeline actors. If we're still recording,
    // juse use Date.now() for the memory and timeline end times, as those
    // are only used in tests.
    if (!this.isRecording()) {
      // Check to see if memory is recording, so we only stop recording
      // if necessary (otherwise if the memory component is not attached, this will fail)
      if (this._memory.isRecordingAllocations()) {
        this._memory.stopRecordingAllocations();
      }
      this._timeline.stop();
    }

    let recordingData = {
      // Data available only at the end of a recording.
      profile: profilerData.profile,
      // End times for all the actors.
      duration: profilerData.currentTime - startTime,
    };

    events.emit(this, "recording-stopped", model, recordingData);
    return model;
  }),

  /**
   * Checks all currently stored recording handles and returns a boolean
   * if there is a session currently being recorded.
   *
   * @return Boolean
   */
  isRecording: function () {
    return this._recordings.some(h => h.isRecording());
  },

  /**
   * Returns all current recordings.
   */
  getRecordings: function () {
    return this._recordings;
  },

  /**
   * Sets how often the "profiler-status" event should be emitted.
   * Used in tests.
   */
  setProfilerStatusInterval: function (n) {
    this._profiler.setProfilerStatusInterval(n);
  },

  /**
   * Returns the configurations set on underlying components, used in tests.
   * Returns an object with `probability`, `maxLogLength` for allocations, and
   * `features`, `threadFilters`, `entries` and `interval` for profiler.
   *
   * @return {object}
   */
  getConfiguration: function () {
    return extend({}, this._memory.getAllocationsSettings(), this._profiler.getStartOptions());
  },

  toString: () => "[object PerformanceRecorder]"
});

/**
 * Creates an object of configurations based off of preferences for a PerformanceRecording.
 */
function getPerformanceRecordingPrefs () {
  return {
    withMarkers: true,
    withMemory: Services.prefs.getBoolPref("devtools.performance.ui.enable-memory"),
    withTicks: Services.prefs.getBoolPref("devtools.performance.ui.enable-framerate"),
    withAllocations: Services.prefs.getBoolPref("devtools.performance.ui.enable-allocations"),
    allocationsSampleProbability: +Services.prefs.getCharPref("devtools.performance.memory.sample-probability"),
    allocationsMaxLogLength: Services.prefs.getIntPref("devtools.performance.memory.max-log-length")
  };
}
