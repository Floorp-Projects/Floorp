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
 * Object acting as a mediator between the ProfilerController and
 * DebuggerServer.
 */
function ProfilerConnection(client) {
  this.client = client;
}

ProfilerConnection.prototype = {
  actor: null,

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
    this.client.request(message, aCallback);
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
  // Chrome debugging targets have already obtained a reference to the profiler
  // actor.
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
   * Starts the profiler.
   *
   * @param function aCallback
   *        Function to be called once the profiler is started
   *        or we get an error. It will be called with a single
   *        argument: an error object (may be null).
   */
  start: function PC_start(aCallback) {
    this.profiler.startProfiler(function onStart(aResponse) {
      aCallback(aResponse.error);
    });
  },

  /**
   * Stops the profiler.
   *
   * @param function aCallback
   *        Function to be called once the profiler is stopped
   *        or we get an error. It will be called with a single
   *        argument: an error object (may be null).
   */
  stop: function PC_stop(aCallback) {
    this.profiler.getProfileData(function onData(aResponse) {
      let data = aResponse.profile;
      if (aResponse.error) {
        Cu.reportError("Failed to fetch profile data before stopping the profiler.");
      }

      this.profiler.stopProfiler(function onStop(aResponse) {
        aCallback(aResponse.error, data);
      });
    }.bind(this));
  },

  /**
   * Cleanup.
   */
  destroy: function PC_destroy(aCallback) {
    this.profiler.destroy();
    this.profiler = null;
  }
};
