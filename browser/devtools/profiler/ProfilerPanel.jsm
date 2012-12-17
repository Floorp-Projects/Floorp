/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource:///modules/devtools/ProfilerController.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["ProfilerPanel"];

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", function () {
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
  return DebuggerServer;
});

/**
 * An instance of a profile UI. Profile UI consists of
 * an iframe with Cleopatra loaded in it and some
 * surrounding meta-data (such as uids).
 *
 * Its main function is to talk to the Cleopatra instance
 * inside of the iframe.
 *
 * ProfileUI is also an event emitter. Currently, it emits
 * only one event, 'ready', when Cleopatra is done loading.
 * You can also check 'isReady' property to see if a
 * particular instance has been loaded yet.
 *
 * @param number uid
 *   Unique ID for this profile.
 * @param ProfilerPanel panel
 *   A reference to the container panel.
 */
function ProfileUI(uid, panel) {
  let doc = panel.document;
  let win = panel.window;

  new EventEmitter(this);

  this.isReady = false;
  this.panel = panel;
  this.uid = uid;

  this.iframe = doc.createElement("iframe");
  this.iframe.setAttribute("flex", "1");
  this.iframe.setAttribute("id", "profiler-cleo-" + uid);
  this.iframe.setAttribute("src", "devtools/cleopatra.html?" + uid);
  this.iframe.setAttribute("hidden", "true");

  // Append our iframe and subscribe to postMessage events.
  // They'll tell us when the underlying page is done loading
  // or when user clicks on start/stop buttons.

  doc.getElementById("profiler-report").appendChild(this.iframe);
  win.addEventListener("message", function (event) {
    if (parseInt(event.data.uid, 10) !== parseInt(this.uid, 10)) {
      return;
    }

    switch (event.data.status) {
      case "loaded":
        this.isReady = true;
        this.emit("ready");
        break;
      case "start":
        // Start profiling and, once started, notify the
        // underlying page so that it could update the UI.
        this.panel.startProfiling(function onStart() {
          var data = JSON.stringify({task: "onStarted"});
          this.iframe.contentWindow.postMessage(data, "*");
        }.bind(this));

        break;
      case "stop":
        // Stop profiling and, once stopped, notify the
        // underlying page so that it could update the UI.
        this.panel.stopProfiling(function onStop() {
          var data = JSON.stringify({task: "onStopped"});
          this.iframe.contentWindow.postMessage(data, "*");
        }.bind(this));
    }
  }.bind(this));
}

ProfileUI.prototype = {
  show: function PUI_show() {
    this.iframe.removeAttribute("hidden");
  },

  hide: function PUI_hide() {
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
  parse: function PUI_parse(data, onParsed) {
    if (!this.isReady) {
      return;
    }

    let win = this.iframe.contentWindow;

    win.postMessage(JSON.stringify({
      task: "receiveProfileData",
      rawProfile: data
    }), "*");

    let poll = function pollBreadcrumbs() {
      let wait = this.panel.window.setTimeout.bind(null, poll, 100);
      let trail = win.gBreadcrumbTrail;

      if (!trail) {
        return wait();
      }

      if (!trail._breadcrumbs || !trail._breadcrumbs.length) {
        return wait();
      }

      onParsed();
    }.bind(this);

    poll();
  },

  /**
   * Destroys the ProfileUI instance.
   */
  destroy: function PUI_destroy() {
    this.isReady = null
    this.panel = null;
    this.uid = null;
    this.iframe = null;
  }
};

/**
 * Profiler panel. It is responsible for creating and managing
 * different profile instances (see ProfileUI).
 *
 * ProfilerPanel is an event emitter. It can emit the following
 * events:
 *
 *   - ready:     after the panel is done loading everything,
 *                including the default profile instance.
 *   - started:   after the panel successfuly starts our SPS
 *                profiler.
 *   - stopped:   after the panel successfuly stops our SPS
 *                profiler and is ready to hand over profiling
 *                data
 *   - parsed:    after Cleopatra finishes parsing profiling
 *                data.
 *   - destroyed: after the panel cleans up after itself and
 *                is ready to be destroyed.
 *
 * The following events are used mainly by tests to prevent
 * accidential oranges:
 *
 *   - profileCreated:  after a new profile is created.
 *   - profileSwitched: after user switches to a different
 *                      profile.
 */
function ProfilerPanel(frame, toolbox) {
  this.isReady = false;
  this.window = frame.window;
  this.document = frame.document;
  this.target = toolbox.target;
  this.controller = new ProfilerController();

  this.profiles = new Map();
  this._uid = 0;

  new EventEmitter(this);
}

ProfilerPanel.prototype = {
  isReady:    null,
  window:     null,
  document:   null,
  target:     null,
  controller: null,
  profiles:   null,

  _uid:       null,
  _activeUid: null,

  get activeProfile() {
    return this.profiles.get(this._activeUid);
  },

  set activeProfile(profile) {
    this._activeUid = profile.uid;
  },

  /**
   * Open a debug connection and, on success, switch to
   * the newly created profile.
   *
   * @return Promise
   */
  open: function PP_open() {
    let deferred = Promise.defer();

    this.controller.connect(function onConnect() {
      let create = this.document.getElementById("profiler-create");
      create.addEventListener("click", this.createProfile.bind(this), false);
      create.removeAttribute("disabled");

      let profile = this.createProfile();
      this.switchToProfile(profile, function () {
        this.isReady = true;
        this.emit("ready");

        deferred.resolve(this);
      }.bind(this))
    }.bind(this));

    return deferred.promise;
  },

  /**
   * Creates a new profile instance (see ProfileUI) and
   * adds an appropriate item to the sidebar. Note that
   * this method doesn't automatically switch user to
   * the newly created profile, they have do to switch
   * explicitly.
   *
   * @return ProfilerPanel
   */
  createProfile: function PP_addProfile() {
    let uid  = ++this._uid;
    let list = this.document.getElementById("profiles-list");
    let item = this.document.createElement("li");
    let wrap = this.document.createElement("h1");

    item.setAttribute("id", "profile-" + uid);
    item.setAttribute("data-uid", uid);
    item.addEventListener("click", function (ev) {
      let uid = parseInt(ev.target.getAttribute("data-uid"), 10);
      this.switchToProfile(this.profiles.get(uid));
    }.bind(this), false);

    wrap.className = "profile-name";
    wrap.textContent = "Profile " + uid;

    item.appendChild(wrap);
    list.appendChild(item);

    let profile = new ProfileUI(uid, this);
    this.profiles.set(uid, profile);

    this.emit("profileCreated", uid);
    return profile;
  },

  /**
   * Switches to a different profile by making its instance an
   * active one.
   *
   * @param ProfileUI profile
   *   A profile instance to switch to.
   * @param function onLoad
   *   A function to call when profile instance is ready.
   *   If the instance is already loaded, onLoad will be
   *   called synchronously.
   */
  switchToProfile: function PP_switchToProfile(profile, onLoad) {
    let doc = this.document;

    if (this.activeProfile) {
      this.activeProfile.hide();
    }

    let active = doc.querySelector("#profiles-list > li.splitview-active");
    if (active) {
      active.className = "";
    }

    doc.getElementById("profile-" + profile.uid).className = "splitview-active";
    profile.show();
    this.activeProfile = profile;

    if (profile.isReady) {
      this.emit("profileSwitched", profile.uid);
      onLoad();
      return;
    }

    profile.once("ready", function () {
      this.emit("profileSwitched", profile.uid);
      onLoad();
    }.bind(this));
  },

  /**
   * Start collecting profile data.
   *
   * @param function onStart
   *   A function to call once we get the message
   *   that profiling had been successfuly started.
   */
  startProfiling: function PP_startProfiling(onStart) {
    this.controller.start(function (err) {
      if (err) {
        Cu.reportError("ProfilerController.start: " + err.message);
        return;
      }

      onStart();
      this.emit("started");
    }.bind(this));
  },

  /**
   * Stop collecting profile data and send it to Cleopatra
   * for parsing.
   *
   * @param function onStop
   *   A function to call once we get the message
   *   that profiling had been successfuly stopped.
   */
  stopProfiling: function PP_stopProfiling(onStop) {
    this.controller.isActive(function (err, isActive) {
      if (err) {
        Cu.reportError("ProfilerController.isActive: " + err.message);
        return;
      }

      if (!isActive) {
        return;
      }

      this.controller.stop(function (err, data) {
        if (err) {
          Cu.reportError("ProfilerController.stop: " + err.message);
          return;
        }

        this.activeProfile.parse(data, function onParsed() {
          this.emit("parsed");
        }.bind(this));

        onStop();
        this.emit("stopped");
      }.bind(this));
    }.bind(this));
  },

  /**
   * Cleanup.
   */
  destroy: function PP_destroy() {
    if (this.profiles) {
      let uid = this._uid;

      while (uid >= 0) {
        if (this.profiles.has(uid)) {
          this.profiles.get(uid).destroy();
          this.profiles.delete(uid);
        }
        uid -= 1;
      }
    }

    if (this.controller) {
      this.controller.destroy();
    }

    this.isReady = null;
    this.window = null;
    this.document = null;
    this.target = null;
    this.controller = null;
    this.profiles = null;
    this._uid = null;
    this._activeUid = null;

    this.emit("destroyed");
  }
};
