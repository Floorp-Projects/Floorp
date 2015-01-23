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
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/toolkit/DevToolsUtils");

loader.lazyImporter(this, "gDevTools",
  "resource:///modules/devtools/gDevTools.jsm");

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
  /**
   * Initializes a connection to the profiler and other miscellaneous actors.
   * If already open, nothing happens.
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

    // Sets `this._profiler`
    yield this._connectProfilerActor();

    // Sets or shims `this._timeline`
    yield this._connectTimelineActor();

    this._connected = true;

    Services.obs.notifyObservers(null, "performance-actors-connection-opened", null);
  }),

  /**
   * Destroys this connection.
   */
  destroy: function () {
    this._disconnectActors();
    this._connected = false;
  },

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
    // Only initialize the timeline front if the respective actor is available.
    // Older Gecko versions don't have an existing implementation, in which case
    // all the methods we need can be easily mocked.
    //
    // If the timeline actor exists, all underlying actors (memory, framerate) exist,
    // with the expected methods and behaviour. If using the Performance tool,
    // and timeline actor does not exist (FxOS devices < Gecko 35),
    // then just use the mocked actor and do not display timeline data.
    //
    // TODO use framework level feature detection from bug 1069673
    if (this._target.form && this._target.form.timelineActor) {
      this._timeline = new TimelineFront(this._target.client, this._target.form);
    } else {
      this._timeline = {
        start: () => 0,
        stop: () => 0,
        isRecording: () => false,
        on: () => null,
        off: () => null,
        once: () => promise.reject(),
        destroy: () => null
      };
    }
  },

  /**
   * Closes the connections to non-profiler actors.
   */
  _disconnectActors: function () {
    this._timeline.destroy();
  },

  /**
   * Sends the request over the remote debugging protocol to the
   * specified actor.
   *
   * @param string actor
   *        The designated actor. Currently supported: "profiler", "timeline".
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
}

PerformanceFront.prototype = {
  /**
   * Manually begins a recording session.
   *
   * @param object timelineOptions
   *        An options object to pass to the timeline front. Supported
   *        properties are `withTicks` and `withMemory`.
   * @return object
   *         A promise that is resolved once recording has started.
   */
  startRecording: Task.async(function*(timelineOptions = {}) {
    let profilerStatus = yield this._request("profiler", "isActive");
    let profilerStartTime;

    // Start the profiler only if it wasn't already active. The built-in
    // nsIPerformance module will be kept recording, because it's the same instance
    // for all targets and interacts with the whole platform, so we don't want
    // to affect other clients by stopping (or restarting) it.
    if (!profilerStatus.isActive) {
      // Extend the profiler options so that protocol.js doesn't modify the original.
      let profilerOptions = extend({}, this._customProfilerOptions);
      yield this._request("profiler", "startProfiler", profilerOptions);
      profilerStartTime = 0;
      this.emit("profiler-activated");
    } else {
      profilerStartTime = profilerStatus.currentTime;
      this.emit("profiler-already-active");
    }

    // The timeline actor is target-dependent, so just make sure it's recording.
    // It won't, however, be available in older Geckos (FF < 35).
    let timelineStartTime = yield this._request("timeline", "start", timelineOptions);

    // Return the start times from the two actors. They will be used to
    // synchronize the profiler and timeline data.
    return {
      profilerStartTime,
      timelineStartTime
    };
  }),

  /**
   * Manually ends the current recording session.
   *
   * @return object
   *         A promise that is resolved once recording has stopped,
   *         with the profiler and timeline data.
   */
  stopRecording: Task.async(function*() {
    let timelineEndTime = yield this._request("timeline", "stop");
    let profilerData = yield this._request("profiler", "getProfile");

    // Return the end times from the two actors. They will be used to
    // synchronize the profiler and timeline data.
    return {
      profile: profilerData.profile,
      profilerEndTime: profilerData.currentTime,
      timelineEndTime: timelineEndTime
    };
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
    features: ["js"]
  }
};

/**
 * A collection of small wrappers promisifying functions invoking callbacks.
 */
function listTabs(client) {
  let deferred = promise.defer();
  client.listTabs(deferred.resolve);
  return deferred.promise;
}

exports.getPerformanceActorsConnection = target => SharedPerformanceActors.forTarget(target);
exports.PerformanceFront = PerformanceFront;
