/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu } = require("chrome");
const Services = require("Services");

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "DevToolsUtils", "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "DeferredTask", "resource://gre/modules/DeferredTask.jsm", true);
loader.lazyRequireGetter(this, "Task", "devtools/shared/task", true);

// Events piped from system observers to Profiler instances.
const PROFILER_SYSTEM_EVENTS = [
  "console-api-profiler",
  "profiler-started",
  "profiler-stopped"
];

// How often the "profiler-status" is emitted by default (in ms)
const BUFFER_STATUS_INTERVAL_DEFAULT = 5000;

loader.lazyGetter(this, "nsIProfilerModule", () => {
  return Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
});

var DEFAULT_PROFILER_OPTIONS = {
  // When using the DevTools Performance Tools, this will be overridden
  // by the pref `devtools.performance.profiler.buffer-size`.
  entries: Math.pow(10, 7),
  // When using the DevTools Performance Tools, this will be overridden
  // by the pref `devtools.performance.profiler.sample-rate-khz`.
  interval: 1,
  features: ["js"],
  threadFilters: ["GeckoMain"]
};

/**
 * Main interface for interacting with nsIProfiler
 */
const ProfilerManager = (function () {
  let consumers = new Set();

  return {

    // How often the "profiler-status" is emitted
    _profilerStatusInterval: BUFFER_STATUS_INTERVAL_DEFAULT,

    // How many subscribers there
    _profilerStatusSubscribers: 0,

    // Has the profiler ever been started by the actor?
    started: false,

    /**
     * The nsIProfiler is target agnostic and interacts with the whole platform.
     * Therefore, special care needs to be given to make sure different profiler
     * consumers (i.e. "toolboxes") don't interfere with each other. Register
     * the profiler actor instances here.
     *
     * @param Profiler instance
     *        A profiler actor class.
     */
    addInstance: function (instance) {
      consumers.add(instance);

      // Lazily register events
      this.registerEventListeners();
    },

    /**
     * Remove the profiler actor instances here.
     *
     * @param Profiler instance
     *        A profiler actor class.
     */
    removeInstance: function (instance) {
      consumers.delete(instance);

      if (this.length < 0) {
        let msg = "Somehow the number of started profilers is now negative.";
        DevToolsUtils.reportException("Profiler", msg);
      }

      if (this.length === 0) {
        this.unregisterEventListeners();
        this.stop();
      }
    },

    /**
     * Starts the nsIProfiler module. Doing so will discard any samples
     * that might have been accumulated so far.
     *
     * @param {number} entries [optional]
     * @param {number} interval [optional]
     * @param {Array<string>} features [optional]
     * @param {Array<string>} threadFilters [description]
     *
     * @return {object}
     */
    start: function (options = {}) {
      let config = this._profilerStartOptions = {
        entries: options.entries || DEFAULT_PROFILER_OPTIONS.entries,
        interval: options.interval || DEFAULT_PROFILER_OPTIONS.interval,
        features: options.features || DEFAULT_PROFILER_OPTIONS.features,
        threadFilters: options.threadFilters || DEFAULT_PROFILER_OPTIONS.threadFilters,
      };

      // The start time should be before any samples we might be
      // interested in.
      let currentTime = nsIProfilerModule.getElapsedTime();

      try {
        nsIProfilerModule.StartProfiler(
          config.entries,
          config.interval,
          config.features,
          config.features.length,
          config.threadFilters,
          config.threadFilters.length
        );
      } catch (e) {
        // For some reason, the profiler couldn't be started. This could happen,
        // for example, when in private browsing mode.
        Cu.reportError(`Could not start the profiler module: ${e.message}`);
        return { started: false, reason: e, currentTime };
      }
      this.started = true;

      this._updateProfilerStatusPolling();

      let { position, totalSize, generation } = this.getBufferInfo();
      return { started: true, position, totalSize, generation, currentTime };
    },

    /**
     * Attempts to stop the nsIProfiler module.
     */
    stop: function () {
      // Actually stop the profiler only if the last client has stopped profiling.
      // Since this is used as a root actor, and the profiler module interacts
      // with the whole platform, we need to avoid a case in which the profiler
      // is stopped when there might be other clients still profiling.
      // Also check for `started` to only stop the profiler when the actor
      // actually started it. This is to prevent stopping the profiler initiated
      // by some other code, like Talos.
      if (this.length <= 1 && this.started) {
        nsIProfilerModule.StopProfiler();
        this.started = false;
      }
      this._updateProfilerStatusPolling();
      return { started: false };
    },

    /**
     * Returns all the samples accumulated since the profiler was started,
     * along with the current time. The data has the following format:
     * {
     *   libs: string,
     *   meta: {
     *     interval: number,
     *     platform: string,
     *     ...
     *   },
     *   threads: [{
     *     samples: [{
     *       frames: [{
     *         line: number,
     *         location: string,
     *         category: number
     *       } ... ],
     *       name: string
     *       responsiveness: number
     *       time: number
     *     } ... ]
     *   } ... ]
     * }
     *
     *
     * @param number startTime
     *        Since the circular buffer will only grow as long as the profiler lives,
     *        the buffer can contain unwanted samples. Pass in a `startTime` to only
     *        retrieve samples that took place after the `startTime`, with 0 being
     *        when the profiler just started.
     * @param boolean stringify
     *        Whether or not the returned profile object should be a string or not to
     *        save JSON parse/stringify cycle if emitting over RDP.
     */
    getProfile: function (options) {
      let startTime = options.startTime || 0;
      let profile = options.stringify ?
        nsIProfilerModule.GetProfile(startTime) :
        nsIProfilerModule.getProfileData(startTime);

      return { profile: profile, currentTime: nsIProfilerModule.getElapsedTime() };
    },

    /**
     * Returns an array of feature strings, describing the profiler features
     * that are available on this platform. Can be called while the profiler
     * is stopped.
     *
     * @return {object}
     */
    getFeatures: function () {
      return { features: nsIProfilerModule.GetFeatures([]) };
    },

    /**
     * Returns an object with the values of the current status of the
     * circular buffer in the profiler, returning `position`, `totalSize`,
     * and the current `generation` of the buffer.
     *
     * @return {object}
     */
    getBufferInfo: function () {
      let position = {}, totalSize = {}, generation = {};
      nsIProfilerModule.GetBufferInfo(position, totalSize, generation);
      return {
        position: position.value,
        totalSize: totalSize.value,
        generation: generation.value
      };
    },

    /**
     * Returns the configuration used that was originally passed in to start up the
     * profiler. Used for tests, and does not account for others using nsIProfiler.
     *
     * @param {object}
     */
    getStartOptions: function () {
      return this._profilerStartOptions || {};
    },

    /**
     * Verifies whether or not the nsIProfiler module has started.
     * If already active, the current time is also returned.
     *
     * @return {object}
     */
    isActive: function () {
      let isActive = nsIProfilerModule.IsActive();
      let elapsedTime = isActive ? nsIProfilerModule.getElapsedTime() : undefined;
      let { position, totalSize, generation } = this.getBufferInfo();
      return {
        isActive,
        currentTime: elapsedTime,
        position,
        totalSize,
        generation
      };
    },

    /**
     * Returns an array of objects that describes the shared libraries
     * which are currently loaded into our process. Can be called while the
     * profiler is stopped.
     */
    get sharedLibraries() {
      return {
        sharedLibraries: nsIProfilerModule.sharedLibraries
      };
    },

    /**
     * Number of profiler instances.
     *
     * @return {number}
     */
    get length() {
      return consumers.size;
    },

    /**
     * Callback for all observed notifications.
     * @param object subject
     * @param string topic
     * @param object data
     */
    observe: sanitizeHandler(function (subject, topic, data) {
      let details;

      // An optional label may be specified when calling `console.profile`.
      // If that's the case, stringify it and send it over with the response.
      let { action, arguments: args } = subject || {};
      let profileLabel = args && args.length > 0 ? `${args[0]}` : void 0;

      // If the event was generated from `console.profile` or `console.profileEnd`
      // we need to start the profiler right away and then just notify the client.
      // Otherwise, we'll lose precious samples.
      if (topic === "console-api-profiler" &&
          (action === "profile" || action === "profileEnd")) {
        let { isActive, currentTime } = this.isActive();

        // Start the profiler only if it wasn't already active. Otherwise, any
        // samples that might have been accumulated so far will be discarded.
        if (!isActive && action === "profile") {
          this.start();
          details = { profileLabel, currentTime: 0 };
        } else if (!isActive) {
          // Otherwise, if inactive and a call to profile end, do nothing
          // and don't emit event.
          return;
        }

        // Otherwise, the profiler is already active, so just send
        // to the front the current time, label, and the notification
        // adds the action as well.
        details = { profileLabel, currentTime };
      }

      // Propagate the event to the profiler instances that
      // are subscribed to this event.
      this.emitEvent(topic, { subject, topic, data, details });
    }, "ProfilerManager.observe"),

    /**
     * Registers handlers for the following events to be emitted
     * on active Profiler instances:
     *   - "console-api-profiler"
     *   - "profiler-started"
     *   - "profiler-stopped"
     *   - "profiler-status"
     *
     * The ProfilerManager listens to all events, and individual
     * consumers filter which events they are interested in.
     */
    registerEventListeners: function () {
      if (!this._eventsRegistered) {
        PROFILER_SYSTEM_EVENTS.forEach(eventName =>
          Services.obs.addObserver(this, eventName));
        this._eventsRegistered = true;
      }
    },

    /**
     * Unregisters handlers for all system events.
     */
    unregisterEventListeners: function () {
      if (this._eventsRegistered) {
        PROFILER_SYSTEM_EVENTS.forEach(eventName =>
          Services.obs.removeObserver(this, eventName));
        this._eventsRegistered = false;
      }
    },

    /**
     * Takes an event name and additional data and emits them
     * through each profiler instance that is subscribed to the event.
     *
     * @param {string} eventName
     * @param {object} data
     */
    emitEvent: function (eventName, data) {
      let subscribers = Array.from(consumers).filter(c => {
        return c.subscribedEvents.has(eventName);
      });

      for (let subscriber of subscribers) {
        subscriber.emit(eventName, data);
      }
    },

    /**
     * Updates the frequency that the "profiler-status" event is emitted
     * during recording.
     *
     * @param {number} interval
     */
    setProfilerStatusInterval: function (interval) {
      this._profilerStatusInterval = interval;
      if (this._poller) {
        this._poller._delayMs = interval;
      }
    },

    subscribeToProfilerStatusEvents: function () {
      this._profilerStatusSubscribers++;
      this._updateProfilerStatusPolling();
    },

    unsubscribeToProfilerStatusEvents: function () {
      this._profilerStatusSubscribers--;
      this._updateProfilerStatusPolling();
    },

    /**
     * Will enable or disable "profiler-status" events depending on
     * if there are subscribers and if the profiler is current recording.
     */
    _updateProfilerStatusPolling: function () {
      if (this._profilerStatusSubscribers > 0 && nsIProfilerModule.IsActive()) {
        if (!this._poller) {
          this._poller = new DeferredTask(this._emitProfilerStatus.bind(this),
                                          this._profilerStatusInterval, 0);
        }
        this._poller.arm();
      } else if (this._poller) {
        // No subscribers; turn off if it exists.
        this._poller.disarm();
      }
    },

    _emitProfilerStatus: function () {
      this.emitEvent("profiler-status", this.isActive());
      this._poller.arm();
    }
  };
})();

/**
 * The profiler actor provides remote access to the built-in nsIProfiler module.
 */
class Profiler {
  constructor() {
    EventEmitter.decorate(this);

    this.subscribedEvents = new Set();
    ProfilerManager.addInstance(this);
  }

  destroy() {
    this.unregisterEventNotifications({ events: Array.from(this.subscribedEvents) });
    this.subscribedEvents = null;

    ProfilerManager.removeInstance(this);
  }

  /**
   * @see ProfilerManager.start
   */
  start(options) {
    return ProfilerManager.start(options);
  }

  /**
   * @see ProfilerManager.stop
   */
  stop() {
    return ProfilerManager.stop();
  }

  /**
   * @see ProfilerManager.getProfile
   */
  getProfile(request = {}) {
    return ProfilerManager.getProfile(request);
  }

  /**
   * @see ProfilerManager.getFeatures
   */
  getFeatures() {
    return ProfilerManager.getFeatures();
  }

  /**
   * @see ProfilerManager.getBufferInfo
   */
  getBufferInfo() {
    return ProfilerManager.getBufferInfo();
  }

  /**
   * @see ProfilerManager.getStartOptions
   */
  getStartOptions() {
    return ProfilerManager.getStartOptions();
  }

  /**
   * @see ProfilerManager.isActive
   */
  isActive() {
    return ProfilerManager.isActive();
  }

  /**
   * @see ProfilerManager.sharedLibraries
   */
  sharedLibraries() {
    return ProfilerManager.sharedLibraries;
  }

  /**
   * @see ProfilerManager.setProfilerStatusInterval
   */
  setProfilerStatusInterval(interval) {
    return ProfilerManager.setProfilerStatusInterval(interval);
  }

  /**
   * Subscribes this instance to one of several events defined in
   * an events array.
   * - "console-api-profiler",
   * - "profiler-started",
   * - "profiler-stopped"
   * - "profiler-status"
   *
   * @param {Array<string>} data.event
   * @return {object}
   */
  registerEventNotifications(data = {}) {
    let response = [];
    (data.events || []).forEach(e => {
      if (!this.subscribedEvents.has(e)) {
        if (e === "profiler-status") {
          ProfilerManager.subscribeToProfilerStatusEvents();
        }
        this.subscribedEvents.add(e);
        response.push(e);
      }
    });
    return { registered: response };
  }

  /**
   * Unsubscribes this instance to one of several events defined in
   * an events array.
   *
   * @param {Array<string>} data.event
   * @return {object}
   */
  unregisterEventNotifications(data = {}) {
    let response = [];
    (data.events || []).forEach(e => {
      if (this.subscribedEvents.has(e)) {
        if (e === "profiler-status") {
          ProfilerManager.unsubscribeToProfilerStatusEvents();
        }
        this.subscribedEvents.delete(e);
        response.push(e);
      }
    });
    return { registered: response };
  }

  /**
   * Checks whether or not the profiler module can currently run.
   * @return boolean
   */
  static canProfile() {
    return nsIProfilerModule.CanProfile();
  }
}

/**
 * JSON.stringify callback used in Profiler.prototype.observe.
 */
function cycleBreaker(key, value) {
  if (key == "wrappedJSObject") {
    return undefined;
  }
  return value;
}

/**
 * Create JSON objects suitable for transportation across the RDP,
 * by breaking cycles and making a copy of the `subject` and `data` via
 * JSON.stringifying those values with a replacer that omits properties
 * known to introduce cycles, and then JSON.parsing the result.
 * This spends some CPU cycles, but it's simple.
 *
 * @TODO Also wraps it in a `makeInfallible` -- is this still necessary?
 *
 * @param {function} handler
 * @return {function}
 */
function sanitizeHandler(handler, identifier) {
  return DevToolsUtils.makeInfallible(function (subject, topic, data) {
    subject = (subject && !Cu.isXrayWrapper(subject) && subject.wrappedJSObject)
              || subject;
    subject = JSON.parse(JSON.stringify(subject, cycleBreaker));
    data = (data && !Cu.isXrayWrapper(data) && data.wrappedJSObject) || data;
    data = JSON.parse(JSON.stringify(data, cycleBreaker));

    // Pass in clean data to the underlying handler
    return handler.call(this, subject, topic, data);
  }, identifier);
}

exports.Profiler = Profiler;
