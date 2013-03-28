/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");

let EXPORTED_SYMBOLS = ["ProfilerController"];

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", function () {
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
  return DebuggerServer;
});

/**
 * Makes a structure representing an individual profile.
 */
function makeProfile(name) {
  return {
    name: name,
    timeStarted: null,
    timeEnded: null
  };
}

/**
 * Object acting as a mediator between the ProfilerController and
 * DebuggerServer.
 */
function ProfilerConnection(client) {
  this.client = client;
  this.startTime = 0;
}

ProfilerConnection.prototype = {
  actor: null,
  startTime: null,

  /**
   * Returns how many milliseconds have passed since the connection
   * was started (start time is specificed by the startTime property).
   *
   * @return number
   */
  get currentTime() {
    return (new Date()).getTime() - this.startTime;
  },

  /**
   * Connects to a debugee and executes a callback when ready.
   *
   * @param function aCallback
   *        Function to be called once we're connected to the client.
   */
  connect: function PCn_connect(aCallback) {
    this.client.listTabs(function (aResponse) {
      this.actor = aResponse.profilerActor;
      aCallback();
    }.bind(this));
  },

  /**
   * Sends a message to check if the profiler is currently active.
   *
   * @param function aCallback
   *        Function to be called once we have a response from
   *        the client. It will be called with a single argument
   *        containing a response object.
   */
  isActive: function PCn_isActive(aCallback) {
    var message = { to: this.actor, type: "isActive" };
    this.client.request(message, aCallback);
  },

  /**
   * Sends a message to start a profiler.
   *
   * @param function aCallback
   *        Function to be called once the profiler is running.
   *        It will be called with a single argument containing
   *        a response object.
   */
  startProfiler: function PCn_startProfiler(aCallback) {
    var message = {
      to: this.actor,
      type: "startProfiler",
      entries: 1000000,
      interval: 1,
      features: ["js"],
    };

    this.client.request(message, function () {
      // Record the current time so we could split profiler data
      // in chunks later.
      this.startTime = (new Date()).getTime();
      aCallback.apply(null, Array.slice(arguments));
    }.bind(this));
  },

  /**
   * Sends a message to stop a profiler.
   *
   * @param function aCallback
   *        Function to be called once the profiler is idle.
   *        It will be called with a single argument containing
   *        a response object.
   */
  stopProfiler: function PCn_stopProfiler(aCallback) {
    var message = { to: this.actor, type: "stopProfiler" };
    this.client.request(message, aCallback);
  },

  /**
   * Sends a message to get the generated profile data.
   *
   * @param function aCallback
   *        Function to be called once we have the data.
   *        It will be called with a single argument containing
   *        a response object.
   */
  getProfileData: function PCn_getProfileData(aCallback) {
    var message = { to: this.actor, type: "getProfile" };
    this.client.request(message, aCallback);
  },

  /**
   * Cleanup.
   */
  destroy: function PCn_destroy() {
    this.client = null;
  }
};

/**
 * Object defining the profiler controller components.
 */
function ProfilerController(target) {
  this.profiler = new ProfilerConnection(target.client);
  this.profiles = new Map();

  // Chrome debugging targets have already obtained a reference to the
  // profiler actor.
  this._connected = !!target.chrome;

  if (target.chrome) {
    this.profiler.actor = target.form.profilerActor;
  }
}

ProfilerController.prototype = {
  /**
   * Connects to the client unless we're already connected.
   *
   * @param function aCallback
   *        Function to be called once we're connected. If
   *        the controller is already connected, this function
   *        will be called immediately (synchronously).
   */
  connect: function (aCallback) {
    if (this._connected) {
      return void aCallback();
    }

    this.profiler.connect(function onConnect() {
      this._connected = true;
      aCallback();
    }.bind(this));
  },

  /**
   * Checks whether the profiler is active.
   *
   * @param function aCallback
   *        Function to be called with a response from the
   *        client. It will be called with two arguments:
   *        an error object (may be null) and a boolean
   *        value indicating if the profiler is active or not.
   */
  isActive: function PC_isActive(aCallback) {
    this.profiler.isActive(function onActive(aResponse) {
      aCallback(aResponse.error, aResponse.isActive);
    });
  },

  /**
   * Checks whether the profile is currently recording.
   *
   * @param object profile
   *        An object made by calling makeProfile function.
   * @return boolean
   */
  isProfileRecording: function PC_isProfileRecording(profile) {
    return profile.timeStarted !== null && profile.timeEnded === null;
  },

  /**
   * Creates a new profile and starts the profiler, if needed.
   *
   * @param string name
   *        Name of the profile.
   * @param function cb
   *        Function to be called once the profiler is started
   *        or we get an error. It will be called with a single
   *        argument: an error object (may be null).
   */
  start: function PC_start(name, cb) {
    if (this.profiles.has(name)) {
      return;
    }

    let profiler = this.profiler;
    let profile = makeProfile(name);
    this.profiles.set(name, profile);


    // If profile is already running, no need to do anything.
    if (this.isProfileRecording(profile)) {
      return void cb();
    }

    this.isActive(function (err, isActive) {
      if (isActive) {
        profile.timeStarted = profiler.currentTime;
        return void cb();
      }

      profiler.startProfiler(function onStart(aResponse) {
        if (aResponse.error) {
          return void cb(aResponse.error);
        }

        profile.timeStarted = profiler.currentTime;
        cb();
      });
    });
  },

  /**
   * Stops the profiler.
   *
   * @param string name
   *        Name of the profile that needs to be stopped.
   * @param function cb
   *        Function to be called once the profiler is stopped
   *        or we get an error. It will be called with a single
   *        argument: an error object (may be null).
   */
  stop: function PC_stop(name, cb) {
    let profiler = this.profiler;
    let profile = this.profiles.get(name);

    if (!profile || !this.isProfileRecording(profile)) {
      return;
    }

    let isRecording = function () {
      for (let [ name, profile ] of this.profiles) {
        if (this.isProfileRecording(profile)) {
          return true;
        }
      }

      return false;
    }.bind(this);

    let onStop = function (data) {
      if (isRecording()) {
        return void cb(null, data);
      }

      profiler.stopProfiler(function onStopProfiler(response) {
        cb(response.error, data);
      });
    }.bind(this);

    profiler.getProfileData(function onData(aResponse) {
      if (aResponse.error) {
        Cu.reportError("Failed to fetch profile data before stopping the profiler.");
        return void cb(aResponse.error, null);
      }

      let data = aResponse.profile;
      profile.timeEnded = profiler.currentTime;

      data.threads = data.threads.map(function (thread) {
        let samples = thread.samples.filter(function (sample) {
          return sample.time >= profile.timeStarted;
        });
        return { samples: samples };
      });

      onStop(data);
    });
  },

  /**
   * Cleanup.
   */
  destroy: function PC_destroy() {
    this.profiler.destroy();
    this.profiler = null;
  }
};
