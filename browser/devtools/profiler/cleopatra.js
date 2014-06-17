/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { Cu }       = require("chrome");
let { defer }    = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;
let EventEmitter = require("devtools/toolkit/event-emitter");

const { PROFILE_IDLE, PROFILE_COMPLETED, PROFILE_RUNNING } = require("devtools/profiler/consts");

/**
 * An implementation of a profile visualization that uses Cleopatra.
 * It consists of an iframe with Cleopatra loaded in it and some
 * surrounding meta-data (such as UIDs).
 *
 * Cleopatra is also an event emitter. It emits the following events:
 *  - ready, when Cleopatra is done loading (you can also check the isReady
 *    property to see if a particular instance has been loaded yet.
 *
 * @param number uid
 *   Unique ID for this profile.
 * @param ProfilerPanel panel
 *   A reference to the container panel.
 */
function Cleopatra(panel, opts) {
  let doc = panel.document;
  let win = panel.window;
  let { uid, name } = opts;
  let spd = opts.showPlatformData;
  let ext = opts.external;

  EventEmitter.decorate(this);

  this.isReady = false;
  this.isStarted = false;
  this.isFinished = false;

  this.panel = panel;
  this.uid = uid;
  this.name = name;

  this.iframe = doc.createElement("iframe");
  this.iframe.setAttribute("flex", "1");
  this.iframe.setAttribute("id", "profiler-cleo-" + uid);
  this.iframe.setAttribute("src", "cleopatra.html?uid=" + uid + "&spd=" + spd + "&ext=" + ext);
  this.iframe.setAttribute("hidden", "true");

  // Append our iframe and subscribe to postMessage events.
  // They'll tell us when the underlying page is done loading
  // or when user clicks on start/stop buttons.

  doc.getElementById("profiler-report").appendChild(this.iframe);
  win.addEventListener("message", (event) => {
    if (parseInt(event.data.uid, 10) !== parseInt(this.uid, 10)) {
      return;
    }

    switch (event.data.status) {
      case "loaded":
        this.isReady = true;
        this.emit("ready");
        break;
      case "displaysource":
        this.panel.displaySource(event.data.data);
    }
  });
}

Cleopatra.prototype = {
  /**
   * Returns a contentWindow of the iframe pointing to Cleopatra
   * if it exists and can be accessed. Otherwise returns null.
   */
  get contentWindow() {
    if (!this.iframe) {
      return null;
    }

    try {
      return this.iframe.contentWindow;
    } catch (err) {
      return null;
    }
  },

  show: function () {
    this.iframe.removeAttribute("hidden");
  },

  hide: function () {
    this.iframe.setAttribute("hidden", true);
  },

  /**
   * Send raw profiling data to Cleopatra for parsing.
   *
   * @param object data
   *   Raw profiling data from the SPS Profiler.
   * @param function onParsed
   *   A callback to be called when Cleopatra finishes
   *   parsing and displaying results.
   *
   */
  parse: function (data, onParsed) {
    if (!this.isReady) {
      return void this.on("ready", this.parse.bind(this, data, onParsed));
    }

    this.message({ task: "receiveProfileData", rawProfile: data }).then(() => {
      let poll = () => {
        let wait = this.panel.window.setTimeout.bind(null, poll, 100);
        let trail = this.contentWindow.gBreadcrumbTrail;

        if (!trail) {
          return wait();
        }

        if (!trail._breadcrumbs || !trail._breadcrumbs.length) {
          return wait();
        }

        onParsed();
      };

      poll();
    });
  },

  /**
   * Send a message to Cleopatra instance. If a message cannot be
   * sent, this method queues it for later.
   *
   * @param object data JSON data to send (must be serializable)
   * @return promise
   */
  message: function (data) {
    let deferred = defer();
    data = JSON.stringify(data);

    let send = () => {
      if (!this.contentWindow)
        setTimeout(send, 50);

      this.contentWindow.postMessage(data, "*");
      deferred.resolve();
    };

    send();
    return deferred.promise;
  },

  /**
   * Destroys the ProfileUI instance.
   */
  destroy: function () {
    this.isReady = null;
    this.panel = null;
    this.uid = null;
    this.iframe = null;
    this.messages = null;
  }
};

module.exports = Cleopatra;

