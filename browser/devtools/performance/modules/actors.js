/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
const { Promise } = require("resource://gre/modules/Promise.jsm");
const {
  actorCompatibilityBridge, getProfiler,
  MockMemoryFront, MockTimelineFront,
  memoryActorSupported, timelineActorSupported
} = require("devtools/performance/compatibility");

loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/performance/recording-utils", true);
loader.lazyRequireGetter(this, "TimelineFront",
  "devtools/server/actors/timeline", true);
loader.lazyRequireGetter(this, "MemoryFront",
  "devtools/server/actors/memory", true);
loader.lazyRequireGetter(this, "timers",
  "resource://gre/modules/Timer.jsm");

// how often do we pull allocation sites from the memory actor
const ALLOCATION_SITE_POLL_TIMER = 200; // ms

const MEMORY_ACTOR_METHODS = [
  "destroy", "attach", "detach", "getState", "getAllocationsSettings",
  "getAllocations", "startRecordingAllocations", "stopRecordingAllocations"
];

const TIMELINE_ACTOR_METHODS = [
  "start", "stop",
];

const PROFILER_ACTOR_METHODS = [
  "isActive", "startProfiler", "getStartOptions", "stopProfiler",
  "registerEventNotifications", "unregisterEventNotifications"
];

/**
 * Constructor for a facade around an underlying ProfilerFront.
 */
function ProfilerFrontFacade (target) {
  this._target = target;
  this._onProfilerEvent = this._onProfilerEvent.bind(this);
  EventEmitter.decorate(this);
}

ProfilerFrontFacade.prototype = {
  EVENTS: ["console-api-profiler", "profiler-stopped"],

  // Connects to the targets underlying real ProfilerFront.
  connect: Task.async(function*() {
    let target = this._target;
    this._actor = yield getProfiler(target);

    // Fetch and store information about the SPS profiler and
    // server profiler.
    this.traits = {};
    this.traits.filterable = target.getTrait("profilerDataFilterable");

    // Directly register to event notifications when connected
    // to hook into `console.profile|profileEnd` calls.
    yield this.registerEventNotifications({ events: this.EVENTS });
    // TODO bug 1159389, listen directly to actor if supporting new front
    target.client.addListener("eventNotification", this._onProfilerEvent);
  }),

  /**
   * Unregisters events for the underlying profiler actor.
   */
  destroy: Task.async(function *() {
    yield this.unregisterEventNotifications({ events: this.EVENTS });
    // TODO bug 1159389, listen directly to actor if supporting new front
    this._target.client.removeListener("eventNotification", this._onProfilerEvent);
  }),

  /**
   * Starts the profiler actor, if necessary.
   */
  start: Task.async(function *(options={}) {
    // Start the profiler only if it wasn't already active. The built-in
    // nsIPerformance module will be kept recording, because it's the same instance
    // for all targets and interacts with the whole platform, so we don't want
    // to affect other clients by stopping (or restarting) it.
    let profilerStatus = yield this.isActive();
    if (profilerStatus.isActive) {
      this.emit("profiler-already-active");
      return profilerStatus.currentTime;
    }

    // Translate options from the recording model into profiler-specific
    // options for the nsIProfiler
    let profilerOptions = {
      entries: options.bufferSize,
      interval: options.sampleFrequency ? (1000 / (options.sampleFrequency * 1000)) : void 0
    };

    yield this.startProfiler(profilerOptions);

    this.emit("profiler-activated");
    return 0;
  }),

  /**
   * Returns profile data from now since `startTime`.
   */
  getProfile: Task.async(function *(options) {
    let profilerData = yield (actorCompatibilityBridge("getProfile").call(this, options));
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
  _onProfilerEvent: function (_, { topic, subject, details }) {
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

  toString: () => "[object ProfilerFrontFacade]"
};

// Bind all the methods that directly proxy to the actor
PROFILER_ACTOR_METHODS.forEach(method => ProfilerFrontFacade.prototype[method] = actorCompatibilityBridge(method));
exports.ProfilerFront = ProfilerFrontFacade;

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
    let supported = yield timelineActorSupported(this._target);
    this._actor = supported ?
                  new TimelineFront(this._target.client, this._target.form) :
                  new MockTimelineFront();

    this.IS_MOCK = !supported;

    // Binds underlying actor events and consolidates them to a `timeline-data`
    // exposed event.
    this.EVENTS.forEach(type => {
      let handler = this[`_on${type}`] = this._onTimelineData.bind(this, type);
      this._actor.on(type, handler);
    });
  }),

  /**
   * Override actor's destroy, so we can unregister listeners before
   * destroying the underlying actor.
   */
  destroy: Task.async(function *() {
    this.EVENTS.forEach(type => this._actor.off(type, this[`_on${type}`]));
    yield this._actor.destroy();
  }),

  /**
   * An aggregate of all events (markers, frames, memory, ticks) and exposes
   * to PerformanceActorsConnection as a single event.
   */
  _onTimelineData: function (type, ...data) {
    this.emit("timeline-data", type, ...data);
  },

  toString: () => "[object TimelineFrontFacade]"
};

// Bind all the methods that directly proxy to the actor
TIMELINE_ACTOR_METHODS.forEach(method => TimelineFrontFacade.prototype[method] = actorCompatibilityBridge(method));
exports.TimelineFront = TimelineFrontFacade;

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
    let supported = yield memoryActorSupported(this._target);
    this._actor = supported ?
                  new MemoryFront(this._target.client, this._target.form) :
                  new MockMemoryFront();

    this.IS_MOCK = !supported;
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

    yield this._pullAllocationSites();

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
    yield this._lastPullAllocationSitesFinished;
    timers.clearTimeout(this._sitesPullTimeout);

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
    let { promise, resolve } = Promise.defer();
    this._lastPullAllocationSitesFinished = promise;

    if ((yield this.getState()) !== "attached") {
      resolve();
      return;
    }

    let memoryData = yield this.getAllocations();
    // Match the signature of the TimelineFront events, with "timeline-data"
    // being the event name, and the second argument describing the type.
    this.emit("timeline-data", "allocations", {
      sites: memoryData.allocations,
      timestamps: memoryData.allocationsTimestamps,
      frames: memoryData.frames,
      counts: memoryData.counts
    });

    this._sitesPullTimeout = timers.setTimeout(this._pullAllocationSites, ALLOCATION_SITE_POLL_TIMER);

    resolve();
  }),

  toString: () => "[object MemoryFrontFacade]"
};

// Bind all the methods that directly proxy to the actor
MEMORY_ACTOR_METHODS.forEach(method => MemoryFrontFacade.prototype[method] = actorCompatibilityBridge(method));
exports.MemoryFront = MemoryFrontFacade;
