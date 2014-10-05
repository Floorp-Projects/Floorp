/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "FramerateFront",
  "devtools/server/actors/framerate", true);
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/toolkit/DevToolsUtils");

loader.lazyImporter(this, "gDevTools",
  "resource:///modules/devtools/gDevTools.jsm");

/**
 * A cache of all ProfilerConnection instances. The keys are Toolbox objects.
 */
let SharedProfilerConnection = new WeakMap();

/**
 * Instantiates a shared ProfilerConnection for the specified toolbox.
 * Consumers must yield on `open` to make sure the connection is established.
 *
 * @param Toolbox toolbox
 *        The toolbox owning this connection.
 */
SharedProfilerConnection.forToolbox = function(toolbox) {
  if (this.has(toolbox)) {
    return this.get(toolbox);
  }

  let instance = new ProfilerConnection(toolbox);
  this.set(toolbox, instance);
  return instance;
};

/**
 * A connection to the profiler actor, along with other miscellaneous actors,
 * shared by all tools in a toolbox.
 *
 * Use `SharedProfilerConnection.forToolbox` to make sure you get the same
 * instance every time, and the `ProfilerFront` to start/stop recordings.
 *
 * @param Toolbox toolbox
 *        The toolbox owning this connection.
 */
function ProfilerConnection(toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._target = this._toolbox.target;
  this._client = this._target.client;
  this._request = this._request.bind(this);

  this._pendingFramerateConsumers = 0;
  this._pendingConsoleRecordings = [];
  this._finishedConsoleRecordings = [];
  this._onEventNotification = this._onEventNotification.bind(this);

  Services.obs.notifyObservers(null, "profiler-connection-created", null);
}

ProfilerConnection.prototype = {
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

    // Chrome debugging targets have already obtained a reference
    // to the profiler actor.
    if (this._target.chrome) {
      this._profiler = this._target.form.profilerActor;
    }
    // Or when we are debugging content processes, we already have the tab
    // specific one. Use it immediately.
    else if (this._target.form && this._target.form.profilerActor) {
      this._profiler = this._target.form.profilerActor;
      yield this._registerEventNotifications();
    }
    // Check if we already have a grip to the `listTabs` response object
    // and, if we do, use it to get to the profiler actor.
    else if (this._target.root && this._target.root.profilerActor) {
      this._profiler = this._target.root.profilerActor;
      yield this._registerEventNotifications();
    }
    // Otherwise, call `listTabs`.
    else {
      this._profiler = (yield listTabs(this._client)).profilerActor;
      yield this._registerEventNotifications();
    }

    this._connectMiscActors();
    this._connected = true;

    Services.obs.notifyObservers(null, "profiler-connection-opened", null);
  }),

  /**
   * Destroys this connection.
   */
  destroy: function() {
    this._disconnectMiscActors();
    this._connected = false;
  },

  /**
   * Initializes a connection to miscellaneous actors which are going to be
   * used in tandem with the profiler actor.
   */
  _connectMiscActors: function() {
    // Only initialize the framerate front if the respective actor is available.
    // Older Gecko versions don't have an existing implementation, in which case
    // all the methods we need can be easily mocked.
    if (this._target.form && this._target.form.framerateActor) {
      this._framerate = new FramerateFront(this._target.client, this._target.form);
    } else {
      this._framerate = {
        destroy: () => {},
        startRecording: () => {},
        stopRecording: () => {},
        cancelRecording: () => {},
        isRecording: () => false,
        getPendingTicks: () => null
      };
    }
  },

  /**
   * Closes the connections to miscellaneous actors.
   * @see ProfilerConnection.prototype._connectMiscActors
   */
  _disconnectMiscActors: function() {
    this._framerate.destroy();
  },

  /**
   * Sends the request over the remote debugging protocol to the
   * specified actor.
   *
   * @param string actor
   *        The designated actor. Currently supported: "profiler", "framerate".
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

    // Handle requests to the framerate actor.
    if (actor == "framerate") {
      switch (method) {
      // Only stop recording framerate if there are no other pending consumers.
      // Otherwise, for example, the next time `console.profileEnd` is called
      // there won't be any framerate data available, since we're reusing the
      // same actor for multiple overlapping recordings.
        case "startRecording":
          this._pendingFramerateConsumers++;
          break;
        case "stopRecording":
        case "cancelRecording":
          if (--this._pendingFramerateConsumers > 0) return;
          break;
        // Some versions of the framerate actor don't have a 'getPendingTicks'
        // method available, in which case it's impossible to get all the
        // accumulated framerate data without stopping the recording. Bail out.
        case "getPendingTicks":
          if (method in this._framerate) break;
          return null;
      }
      checkPendingFramerateConsumers(this);
      return this._framerate[method].apply(this._framerate, args);
    }
  },

  /**
   * Starts listening to certain events emitted by the profiler actor.
   *
   * @return object
   *         A promise that is resolved once the notifications are registered.
   */
  _registerEventNotifications: Task.async(function*() {
    let events = ["console-api-profiler", "profiler-stopped"];
    yield this._request("profiler", "registerEventNotifications", { events });
    this._client.addListener("eventNotification", this._onEventNotification);
  }),

  /**
   * Invoked whenever a registered event was emitted by the profiler actor.
   *
   * @param object response
   *        The data received from the backend.
   */
  _onEventNotification: function(event, response) {
    let toolbox = gDevTools.getToolbox(this._target);
    if (toolbox == null) {
      return;
    }
    if (response.topic == "console-api-profiler") {
      let action = response.subject.action;
      let details = response.details;
      if (action == "profile") {
        this.emit("invoked-console-profile", details.profileLabel); // used in tests
        this._onConsoleProfileStart(details);
      } else if (action == "profileEnd") {
        this.emit("invoked-console-profileEnd", details.profileLabel); // used in tests
        this._onConsoleProfileEnd(details);
      }
    } else if (response.topic == "profiler-stopped") {
      this._onProfilerUnexpectedlyStopped();
    }
  },

  /**
   * Invoked whenever `console.profile` is called.
   *
   * @param string profileLabel
   *        The provided string argument if available, undefined otherwise.
   * @param number currentTime
   *        The time (in milliseconds) when the call was made, relative to
   *        when the nsIProfiler module was started.
   */
  _onConsoleProfileStart: Task.async(function*({ profileLabel, currentTime }) {
    let pending = this._pendingConsoleRecordings;
    if (pending.find(e => e.profileLabel == profileLabel)) {
      return;
    }
    // Push this unique `console.profile` call to the stack.
    pending.push({
      profileLabel: profileLabel,
      profilingStartTime: currentTime
    });

    // Profiling was automatically started when `console.profile` was invoked,
    // so we all we have to do is make sure the framerate actor is recording.
    yield this._request("framerate", "startRecording");

    // Signal that a call to `console.profile` actually created a new recording.
    this.emit("profile", profileLabel);
  }),

  /**
   * Invoked whenever `console.profileEnd` is called.
   *
   * @param object profilerData
   *        The profiler data received from the backend.
   */
  _onConsoleProfileEnd: Task.async(function*(profilerData) {
    let pending = this._pendingConsoleRecordings;
    if (pending.length == 0) {
      return;
    }
    // Try to find the corresponding `console.profile` call in the stack
    // with the specified label (be it undefined, or a string).
    let info = pending.find(e => e.profileLabel == profilerData.profileLabel);
    if (info) {
      pending.splice(pending.indexOf(info), 1);
    }
    // If no corresponding `console.profile` call was found, and if no label
    // is specified, pop the most recent entry from the stack.
    else if (!profilerData.profileLabel) {
      info = pending.pop();
      profilerData.profileLabel = info.profileLabel;
    }
    // ...Otherwise this is a call to `console.profileEnd` with a label that
    // doesn't exist on the stack. Bail out.
    else {
      return;
    }

    // The profile is already available when console.profileEnd() was invoked,
    // but we need to filter out all samples that fall out of current profile's
    // range. This is necessary because the profiler is continuously running.
    filterSamples(profilerData, info.profilingStartTime);
    offsetSampleTimes(profilerData, info.profilingStartTime);

    // Fetch the recorded refresh driver ticks, during the same time window
    // as the filtered profiler data.
    let beginAt = findEarliestSampleTime(profilerData);
    let endAt = findOldestSampleTime(profilerData);
    let ticksData = yield this._request("framerate", "getPendingTicks", beginAt, endAt);
    yield this._request("framerate", "cancelRecording");

    // Join all the acquired data and emit it for outside consumers.
    let recordingData = {
      recordingDuration: profilerData.currentTime - info.profilingStartTime,
      profilerData: profilerData,
      ticksData: ticksData
    };
    this._finishedConsoleRecordings.push(recordingData);

    // Signal that a call to `console.profileEnd` actually finished a recording.
    this.emit("profileEnd", recordingData);
  }),

  /**
   * Invoked whenever the built-in profiler module is deactivated. Since this
   * should *never* happen while there's a consumer (i.e. "toolbox") available,
   * treat this notification as being unexpected.
   *
   * This may happen, for example, if the Gecko Profiler add-on is installed
   * (and isn't using the profiler actor over the remote protocol). There's no
   * way to prevent it from stopping the profiler and affecting our tool.
   */
  _onProfilerUnexpectedlyStopped: function() {
    // Pop all pending `console.profile` calls from the stack.
    this._pendingConsoleRecordings.length = 0;
    this.emit("profiler-unexpectedly-stopped");
  }
};

/**
 * A thin wrapper around a shared ProfilerConnection for the parent toolbox.
 * Handles manually starting and stopping a recording.
 *
 * @param ProfilerConnection connection
 *        The shared instance for the parent toolbox.
 */
function ProfilerFront(connection) {
  EventEmitter.decorate(this);

  this._request = connection._request;
  this.pendingConsoleRecordings = connection._pendingConsoleRecordings;
  this.finishedConsoleRecordings = connection._finishedConsoleRecordings;

  connection.on("profile", (e, args) => this.emit(e, args));
  connection.on("profileEnd", (e, args) => this.emit(e, args));
  connection.on("profiler-unexpectedly-stopped", (e, args) => this.emit(e, args));
}

ProfilerFront.prototype = {
  /**
   * Manually begins a recording session.
   *
   * @return object
   *         A promise that is resolved once recording has started.
   */
  startRecording: Task.async(function*() {
    let { isActive, currentTime } = yield this._request("profiler", "isActive");

    // Start the profiler only if it wasn't already active. The built-in
    // nsIProfiler module will be kept recording, because it's the same instance
    // for all toolboxes and interacts with the whole platform, so we don't want
    // to affect other clients by stopping (or restarting) it.
    if (!isActive) {
      yield this._request("profiler", "startProfiler", this._customProfilerOptions);
      this._profilingStartTime = 0;
      this.emit("profiler-activated");
    } else {
      this._profilingStartTime = currentTime;
      this.emit("profiler-already-active");
    }

    // The framerate actor is target-dependent, so just make sure
    // it's recording.
    yield this._request("framerate", "startRecording");
  }),

  /**
   * Manually ends the current recording session.
   *
   * @return object
   *         A promise that is resolved once recording has stopped,
   *         with the profiler and framerate data.
   */
  stopRecording: Task.async(function*() {
    // We'll need to filter out all samples that fall out of current profile's
    // range. This is necessary because the profiler is continuously running.
    let profilerData = yield this._request("profiler", "getProfile");
    filterSamples(profilerData, this._profilingStartTime);
    offsetSampleTimes(profilerData, this._profilingStartTime);

    // Fetch the recorded refresh driver ticks, during the same time window
    // as the filtered profiler data.
    let beginAt = findEarliestSampleTime(profilerData);
    let endAt = findOldestSampleTime(profilerData);
    let ticksData = yield this._request("framerate", "getPendingTicks", beginAt, endAt);
    yield this._request("framerate", "cancelRecording");

    // Join all the aquired data and return it for outside consumers.
    return {
      recordingDuration: profilerData.currentTime - this._profilingStartTime,
      profilerData: profilerData,
      ticksData: ticksData
    };
  }),

  /**
   * Ends the current recording session without trying to retrieve any data.
   */
  cancelRecording: Task.async(function*() {
    yield this._request("framerate", "cancelRecording");
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
 * Filters all the samples in the provided profiler data to be more recent
 * than the specified start time.
 *
 * @param object profilerData
 *        The profiler data received from the backend.
 * @param number profilingStartTime
 *        The earliest acceptable sample time (in milliseconds).
 */
function filterSamples(profilerData, profilingStartTime) {
  let firstThread = profilerData.profile.threads[0];

  firstThread.samples = firstThread.samples.filter(e => {
    return e.time >= profilingStartTime;
  });
}

/**
 * Offsets all the samples in the provided profiler data by the specified time.
 *
 * @param object profilerData
 *        The profiler data received from the backend.
 * @param number timeOffset
 *        The amount of time to offset by (in milliseconds).
 */
function offsetSampleTimes(profilerData, timeOffset) {
  let firstThreadSamples = profilerData.profile.threads[0].samples;

  for (let sample of firstThreadSamples) {
    sample.time -= timeOffset;
  }
}

/**
 * Finds the earliest sample time in the provided profiler data.
 *
 * @param object profilerData
 *        The profiler data received from the backend.
 * @return number
 *         The earliest sample time (in milliseconds).
 */
function findEarliestSampleTime(profilerData) {
  let firstThreadSamples = profilerData.profile.threads[0].samples;

  for (let sample of firstThreadSamples) {
    if ("time" in sample) {
      return sample.time;
    }
  }
}

/**
 * Finds the oldest sample time in the provided profiler data.
 *
 * @param object profilerData
 *        The profiler data received from the backend.
 * @return number
 *         The oldest sample time (in milliseconds).
 */
function findOldestSampleTime(profilerData) {
  let firstThreadSamples = profilerData.profile.threads[0].samples;

  for (let i = firstThreadSamples.length - 1; i >= 0; i--) {
    if ("time" in firstThreadSamples[i]) {
      return firstThreadSamples[i].time;
    }
  }
}

/**
 * Asserts the value sanity of `pendingFramerateConsumers`.
 */
function checkPendingFramerateConsumers(connection) {
  if (connection._pendingFramerateConsumers < 0) {
    let msg = "Somehow the number of framerate consumers is now negative.";
    DevToolsUtils.reportException("ProfilerConnection", msg);
  }
}

/**
 * A collection of small wrappers promisifying functions invoking callbacks.
 */
function listTabs(client) {
  let deferred = promise.defer();
  client.listTabs(deferred.resolve);
  return deferred.promise;
}

exports.getProfilerConnection = toolbox => SharedProfilerConnection.forToolbox(toolbox);
exports.ProfilerFront = ProfilerFront;
