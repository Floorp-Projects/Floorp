/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "Poller",
  "devtools/shared/poller", true);

loader.lazyRequireGetter(this, "CompatUtils",
  "devtools/performance/compatibility");
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/performance/recording-utils");
loader.lazyRequireGetter(this, "TimelineFront",
  "devtools/server/actors/timeline", true);
loader.lazyRequireGetter(this, "MemoryFront",
  "devtools/server/actors/memory", true);
loader.lazyRequireGetter(this, "ProfilerFront",
  "devtools/server/actors/profiler", true);

// how often do we pull allocation sites from the memory actor
const ALLOCATION_SITE_POLL_TIMER = 200; // ms

// how often do we check the status of the profiler's circular buffer
const PROFILER_CHECK_TIMER = 5000; // ms

const MEMORY_ACTOR_METHODS = [
  "attach", "detach", "getState", "getAllocationsSettings",
  "getAllocations", "startRecordingAllocations", "stopRecordingAllocations"
];

const TIMELINE_ACTOR_METHODS = [
  "start", "stop",
];

const PROFILER_ACTOR_METHODS = [
  "startProfiler", "getStartOptions", "stopProfiler",
  "registerEventNotifications", "unregisterEventNotifications"
];

/**
 * Constructor for a facade around an underlying ProfilerFront.
 */
function ProfilerFrontFacade (target) {
  this._target = target;
  this._onProfilerEvent = this._onProfilerEvent.bind(this);
  this._checkProfilerStatus = this._checkProfilerStatus.bind(this);
  this._PROFILER_CHECK_TIMER = this._target.TEST_MOCK_PROFILER_CHECK_TIMER || PROFILER_CHECK_TIMER;

  EventEmitter.decorate(this);
}

ProfilerFrontFacade.prototype = {
  EVENTS: ["console-api-profiler", "profiler-stopped"],

  // Connects to the targets underlying real ProfilerFront.
  connect: Task.async(function*() {
    let target = this._target;
    this._front = new ProfilerFront(target.client, target.form);

    // Fetch and store information about the SPS profiler and
    // server profiler.
    this.traits = {};
    this.traits.filterable = target.getTrait("profilerDataFilterable");

    // Directly register to event notifications when connected
    // to hook into `console.profile|profileEnd` calls.
    yield this.registerEventNotifications({ events: this.EVENTS });
    this.EVENTS.forEach(e => this._front.on(e, this._onProfilerEvent));
  }),

  /**
   * Unregisters events for the underlying profiler actor.
   */
  destroy: Task.async(function *() {
    if (this._poller) {
      yield this._poller.destroy();
    }

    this.EVENTS.forEach(e => this._front.off(e, this._onProfilerEvent));
    yield this.unregisterEventNotifications({ events: this.EVENTS });
    yield this._front.destroy();
  }),

  /**
   * Starts the profiler actor, if necessary.
   *
   * @option {number?} bufferSize
   * @option {number?} sampleFrequency
   */
  start: Task.async(function *(options={}) {
    // Check for poller status even if the profiler is already active --
    // profiler can be activated via `console.profile` or another source, like
    // the Gecko Profiler.
    if (!this._poller) {
      this._poller = new Poller(this._checkProfilerStatus, this._PROFILER_CHECK_TIMER, false);
    }
    if (!this._poller.isPolling()) {
      this._poller.on();
    }

    // Start the profiler only if it wasn't already active. The built-in
    // nsIPerformance module will be kept recording, because it's the same instance
    // for all targets and interacts with the whole platform, so we don't want
    // to affect other clients by stopping (or restarting) it.
    let status = yield this.getStatus();

    // This should only occur during teardown
    if (!status) {
      return;
    }

    let { isActive, currentTime, position, generation, totalSize } = status;

    if (isActive) {
      this.emit("profiler-already-active");
      return { startTime: currentTime, position, generation, totalSize };
    }

    // Translate options from the recording model into profiler-specific
    // options for the nsIProfiler
    let profilerOptions = {
      entries: options.bufferSize,
      interval: options.sampleFrequency ? (1000 / (options.sampleFrequency * 1000)) : void 0
    };

    let startInfo = yield this.startProfiler(profilerOptions);
    let startTime = 0;
    if ("currentTime" in startInfo) {
      startTime = startInfo.currentTime;
    }

    this.emit("profiler-activated");
    return { startTime, position, generation, totalSize };
  }),

  /**
   * Indicates the end of a recording -- does not actually stop the profiler
   * (stopProfiler does that), but notes that we no longer need to poll
   * for buffer status.
   */
  stop: Task.async(function *() {
    yield this._poller.off();
  }),

  /**
   * Wrapper around `profiler.isActive()` to take profiler status data and emit.
   */
  getStatus: Task.async(function *() {
    let data = yield CompatUtils.callFrontMethod("isActive").call(this);
    // If no data, the last poll for `isActive()` was wrapping up, and the target.client
    // is now null, so we no longer have data, so just abort here.
    if (!data) {
      return;
    }

    // If TEST_PROFILER_FILTER_STATUS defined (via array of fields), filter
    // out any field from isActive, used only in tests. Used to filter out
    // buffer status fields to simulate older geckos.
    if (this._target.TEST_PROFILER_FILTER_STATUS) {
      data = Object.keys(data).reduce((acc, prop) => {
        if (this._target.TEST_PROFILER_FILTER_STATUS.indexOf(prop) === -1) {
          acc[prop] = data[prop];
        }
        return acc;
      }, {});
    }

    this.emit("profiler-status", data);
    return data;
  }),

  /**
   * Returns profile data from now since `startTime`.
   */
  getProfile: Task.async(function *(options) {
    let profilerData = yield CompatUtils.callFrontMethod("getProfile").call(this, options);
    // If the backend is not deduped, dedupe it ourselves, as rest of the code
    // expects a deduped profile.
    if (profilerData.profile.meta.version === 2) {
      RecordingUtils.deflateProfile(profilerData.profile);
    }

    // If the backend does not support filtering by start and endtime on platform (< Fx40),
    // do it on the client (much slower).
    if (!this.traits.filterable) {
      RecordingUtils.filterSamples(profilerData.profile, options.startTime || 0);
    }

    return profilerData;
  }),

  /**
   * Invoked whenever a registered event was emitted by the profiler actor.
   *
   * @param object response
   *        The data received from the backend.
   */
  _onProfilerEvent: function (data) {
    let { subject, topic, details } = data;

    if (topic === "console-api-profiler") {
      if (subject.action === "profile") {
        this.emit("console-profile-start", details);
      } else if (subject.action === "profileEnd") {
        this.emit("console-profile-end", details);
      }
    } else if (topic === "profiler-stopped") {
      this.emit("profiler-stopped");
    }
  },

  _checkProfilerStatus: Task.async(function *() {
    // Calling `getStatus()` will emit the "profiler-status" on its own
    yield this.getStatus();
  }),

  toString: () => "[object ProfilerFrontFacade]"
};

/**
 * Constructor for a facade around an underlying TimelineFront.
 */
function TimelineFrontFacade (target) {
  this._target = target;
  EventEmitter.decorate(this);
}

TimelineFrontFacade.prototype = {
  EVENTS: ["markers", "frames", "memory", "ticks"],

  connect: Task.async(function*() {
    let supported = yield CompatUtils.timelineActorSupported(this._target);
    this._front = supported ?
                  new TimelineFront(this._target.client, this._target.form) :
                  new CompatUtils.MockTimelineFront();

    this.IS_MOCK = !supported;

    // Binds underlying actor events and consolidates them to a `timeline-data`
    // exposed event.
    this.EVENTS.forEach(type => {
      let handler = this[`_on${type}`] = this._onTimelineData.bind(this, type);
      this._front.on(type, handler);
    });
  }),

  /**
   * Override actor's destroy, so we can unregister listeners before
   * destroying the underlying actor.
   */
  destroy: Task.async(function *() {
    this.EVENTS.forEach(type => this._front.off(type, this[`_on${type}`]));
    yield this._front.destroy();
  }),

  /**
   * An aggregate of all events (markers, frames, memory, ticks) and exposes
   * to PerformanceFront as a single event.
   */
  _onTimelineData: function (type, ...data) {
    this.emit("timeline-data", type, ...data);
  },

  toString: () => "[object TimelineFrontFacade]"
};

/**
 * Constructor for a facade around an underlying ProfilerFront.
 */
function MemoryFrontFacade (target) {
  this._target = target;
  this._pullAllocationSites = this._pullAllocationSites.bind(this);

  EventEmitter.decorate(this);
}

MemoryFrontFacade.prototype = {
  connect: Task.async(function*() {
    let supported = yield CompatUtils.memoryActorSupported(this._target);
    this._front = supported ?
                  new MemoryFront(this._target.client, this._target.form) :
                  new CompatUtils.MockMemoryFront();

    this.IS_MOCK = !supported;
  }),

  /**
   * Disables polling and destroys actor.
   */
  destroy: Task.async(function *() {
    if (this._poller) {
      yield this._poller.destroy();
    }
    yield this._front.destroy();
  }),

  /**
   * Starts polling for allocation information.
   */
  start: Task.async(function *(options) {
    if (!options.withAllocations) {
      return 0;
    }

    yield this.attach();

    // Reconstruct our options because the server actor fails when being passed
    // undefined values in objects.
    let allocationOptions = {};
    if (options.allocationsSampleProbability !== void 0) {
      allocationOptions.probability = options.allocationsSampleProbability;
    }
    if (options.allocationsMaxLogLength !== void 0) {
      allocationOptions.maxLogLength = options.allocationsMaxLogLength;
    }

    let startTime = yield this.startRecordingAllocations(allocationOptions);

    if (!this._poller) {
      this._poller = new Poller(this._pullAllocationSites, ALLOCATION_SITE_POLL_TIMER, false);
    }
    if (!this._poller.isPolling()) {
      this._poller.on();
    }

    return startTime;
  }),

  /**
   * Stops polling for allocation information.
   */
  stop: Task.async(function *(options) {
    if (!options.withAllocations) {
      return 0;
    }

    // Since `_pullAllocationSites` is usually running inside a timeout, and
    // it's performing asynchronous requests to the server, a recording may
    // be stopped before that method finishes executing. Therefore, we need to
    // wait for the last request to `getAllocations` to finish before actually
    // stopping recording allocations.
    yield this._poller.off();
    yield this._lastPullAllocationSitesFinished;

    let endTime = yield this.stopRecordingAllocations();
    yield this.detach();

    return endTime;
  }),

  /**
   * At regular intervals, pull allocations from the memory actor, and
   * forward them on this Front facade as "timeline-data" events. This
   * gives the illusion that the MemoryActor supports an EventEmitter-style
   * event stream.
   */
  _pullAllocationSites: Task.async(function *() {
    let deferred = promise.defer();
    this._lastPullAllocationSitesFinished = deferred.promise;

    if ((yield this.getState()) !== "attached") {
      deferred.resolve();
      return;
    }

    let memoryData = yield this.getAllocations();
    // Match the signature of the TimelineFront events, with "timeline-data"
    // being the event name, and the second argument describing the type.
    this.emit("timeline-data", "allocations", memoryData);

    deferred.resolve();
  }),

  toString: () => "[object MemoryFrontFacade]"
};

// Bind all the methods that directly proxy to the actor
PROFILER_ACTOR_METHODS.forEach(m => ProfilerFrontFacade.prototype[m] = CompatUtils.callFrontMethod(m));
TIMELINE_ACTOR_METHODS.forEach(m => TimelineFrontFacade.prototype[m] = CompatUtils.callFrontMethod(m));
MEMORY_ACTOR_METHODS.forEach(m => MemoryFrontFacade.prototype[m] = CompatUtils.callFrontMethod(m));

exports.ProfilerFront = ProfilerFrontFacade;
exports.TimelineFront = TimelineFrontFacade;
exports.MemoryFront = MemoryFrontFacade;
