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
 * Data structure that contains information that has
 * to be shared between separate ProfilerController
 * instances.
 */
const sharedData = {
  startTime: 0,
  data: new WeakMap(),
};

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

// Three functions below all operate with sharedData
// structure defined above. They should be self-explanatory.

function addTarget(target) {
  sharedData.data.set(target, new Map());
}

function getProfiles(target) {
  return sharedData.data.get(target);
}

function getCurrentTime() {
  return (new Date()).getTime() - sharedData.startTime;
}

/**
 * Object to control the JavaScript Profiler over the remote
 * debugging protocol.
 *
 * @param Target target
 *        A target object as defined in Target.jsm
 */
function ProfilerController(target) {
  this.target = target;
  this.client = target.client;
  this.isConnected = false;

  addTarget(target);

  // Chrome debugging targets have already obtained a reference
  // to the profiler actor.
  if (target.chrome) {
    this.isConnected = true;
    this.actor = target.form.profilerActor;
  }
};

ProfilerController.prototype = {
  /**
   * Return a map of profile results for the current target.
   *
   * @return Map
   */
  get profiles() {
    return getProfiles(this.target);
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
   * Connects to the client unless we're already connected.
   *
   * @param function cb
   *        Function to be called once we're connected. If
   *        the controller is already connected, this function
   *        will be called immediately (synchronously).
   */
  connect: function (cb) {
    if (this.isConnected) {
      return void cb();
    }

    this.client.listTabs((resp) => {
      this.actor = resp.profilerActor;
      this.isConnected = true;
      cb();
    })
  },

  /**
   * Adds actor and type information to data and sends the request over
   * the remote debugging protocol.
   *
   * @param string type
   *        Method to call on the other side
   * @param object data
   *        Data to send with the request
   * @param function cb
   *        A callback function
   */
  request: function (type, data, cb) {
    data.to = this.actor;
    data.type = type;
    this.client.request(data, cb);
  },

  /**
   * Checks whether the profiler is active.
   *
   * @param function cb
   *        Function to be called with a response from the
   *        client. It will be called with two arguments:
   *        an error object (may be null) and a boolean
   *        value indicating if the profiler is active or not.
   */
  isActive: function (cb) {
    this.request("isActive", {}, (resp) => cb(resp.error, resp.isActive));
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

    let profile = makeProfile(name);
    this.profiles.set(name, profile);

    // If profile is already running, no need to do anything.
    if (this.isProfileRecording(profile)) {
      return void cb();
    }

    this.isActive((err, isActive) => {
      if (isActive) {
        profile.timeStarted = getCurrentTime();
        return void cb();
      }

      let params = {
        entries: 1000000,
        interval: 1,
        features: ["js"],
      };

      this.request("startProfiler", params, (resp) => {
        if (resp.error) {
          return void cb(resp.error);
        }

        sharedData.startTime = (new Date()).getTime();
        profile.timeStarted = getCurrentTime();
        cb();
      });
    });
  },

  /**
   * Stops the profiler. NOTE, that we don't stop the actual
   * SPS Profiler here. It will be stopped as soon as all
   * clients disconnect from the profiler actor.
   *
   * @param string name
   *        Name of the profile that needs to be stopped.
   * @param function cb
   *        Function to be called once the profiler is stopped
   *        or we get an error. It will be called with a single
   *        argument: an error object (may be null).
   */
  stop: function PC_stop(name, cb) {
    if (!this.profiles.has(name)) {
      return;
    }

    let profile = this.profiles.get(name);
    if (!this.isProfileRecording(profile)) {
      return;
    }

    this.request("getProfile", {}, (resp) => {
      if (resp.error) {
        Cu.reportError("Failed to fetch profile data.");
        return void cb(resp.error, null);
      }

      let data = resp.profile;
      profile.timeEnded = getCurrentTime();

      // Filter out all samples that fall out of current
      // profile's range.

      data.threads = data.threads.map((thread) => {
        let samples = thread.samples.filter((sample) => {
          return sample.time >= profile.timeStarted;
        });

        return { samples: samples };
      });

      cb(null, data);
    });
  },

  /**
   * Cleanup.
   */
  destroy: function PC_destroy() {
    this.client = null;
    this.target = null;
    this.actor = null;
  }
};
