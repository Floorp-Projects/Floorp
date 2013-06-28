/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { PROFILE_IDLE, PROFILE_RUNNING, PROFILE_COMPLETED } = require("devtools/profiler/consts");

var EventEmitter = require("devtools/shared/event-emitter");
var Promise      = require("sdk/core/promise");
var Cleopatra    = require("devtools/profiler/cleopatra");
var Sidebar      = require("devtools/profiler/sidebar");
var ProfilerController = require("devtools/profiler/controller");

Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource://gre/modules/devtools/Console.jsm");
Cu.import("resource://gre/modules/Services.jsm")

/**
 * Profiler panel. It is responsible for creating and managing
 * different profile instances (see cleopatra.js).
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

  this.profiles = new Map();
  this._uid = 0;
  this._msgQueue = {};

  EventEmitter.decorate(this);
}

ProfilerPanel.prototype = {
  isReady:     null,
  window:      null,
  document:    null,
  target:      null,
  controller:  null,
  profiles:    null,
  sidebar:     null,

  _uid:        null,
  _activeUid:  null,
  _runningUid: null,
  _browserWin: null,
  _msgQueue:   null,

  get controls() {
    let doc = this.document;

    return {
      get record() doc.querySelector("#profiler-start")
    };
  },

  get activeProfile() {
    return this.profiles.get(this._activeUid);
  },

  set activeProfile(profile) {
    if (this._activeUid === profile.uid)
      return;

    if (this.activeProfile)
      this.activeProfile.hide();

    this._activeUid = profile.uid;
    profile.show();
  },

  set recordingProfile(profile) {
    let btn = this.controls.record;
    this._runningUid = profile ? profile.uid : null;

    if (this._runningUid)
      btn.setAttribute("checked", true)
    else
      btn.removeAttribute("checked");
  },

  get recordingProfile() {
    return this.profiles.get(this._runningUid);
  },

  get browserWindow() {
    if (this._browserWin) {
      return this._browserWin;
    }

    let win = this.window.top;
    let type = win.document.documentElement.getAttribute("windowtype");

    if (type !== "navigator:browser") {
      win = Services.wm.getMostRecentWindow("navigator:browser");
    }

    return this._browserWin = win;
  },

  /**
   * Open a debug connection and, on success, switch to the newly created
   * profile.
   *
   * @return Promise
   */
  open: function PP_open() {
    // Local profiling needs to make the target remote.
    let target = this.target;
    let promise = !target.isRemote ? target.makeRemote() : Promise.resolve(target);

    return promise
      .then((target) => {
        let deferred = Promise.defer();

        this.controller = new ProfilerController(this.target);
        this.sidebar = new Sidebar(this.document.querySelector("#profiles-list"));

        this.sidebar.widget.addEventListener("select", (ev) => {
          if (!ev.detail)
            return;

          let profile = this.profiles.get(ev.detail.attachment.uid);
          this.activeProfile = profile;

          if (profile.isReady) {
            return void this.emit("profileSwitched", profile.uid);
          }

          profile.once("ready", () => {
            this.emit("profileSwitched", profile.uid);
          });
        });

        this.controller.connect(() => {
          let btn = this.controls.record;
          btn.addEventListener("click", () => this.toggleRecording(), false);
          btn.removeAttribute("disabled");

          // Import queued profiles.
          for (let [name, data] of this.controller.profiles) {
            let profile = this.createProfile(name);
            profile.isStarted = false;
            profile.isFinished = true;
            profile.data = data.data;
            profile.parse(data.data, () => this.emit("parsed"));

            this.sidebar.setProfileState(profile, PROFILE_COMPLETED);
            if (!this.sidebar.selectedItem) {
              this.sidebar.selectedItem = this.sidebar.getItemByProfile(profile);
            }
          }

          this.isReady = true;
          this.emit("ready");
          deferred.resolve(this);
        });

        this.controller.on("profileEnd", (_, data) => {
          let profile = this.createProfile(data.name);
          profile.isStarted = false;
          profile.isFinished = true;
          profile.data = data.data;
          profile.parse(data.data, () => this.emit("parsed"));

          this.sidebar.setProfileState(profile, PROFILE_COMPLETED);
          if (!this.sidebar.selectedItem)
            this.sidebar.selectedItem = this.sidebar.getItemByProfile(profile);

          if (this.recordingProfile && !data.fromConsole)
            this.recordingProfile = null;

          this.emit("stopped");
        });

        return deferred.promise;
      })
      .then(null, (reason) =>
        Cu.reportError("ProfilePanel open failed: " + reason.message));
  },

  /**
   * Creates a new profile instance (see cleopatra.js) and
   * adds an appropriate item to the sidebar. Note that
   * this method doesn't automatically switch user to
   * the newly created profile, they have do to switch
   * explicitly.
   *
   * @param string name
   *        (optional) name of the new profile
   *
   * @return ProfilerPanel
   */
  createProfile: function (name) {
    if (name && this.getProfileByName(name)) {
      return this.getProfileByName(name);
    }

    let uid = ++this._uid;
    let name = name || this.controller.getProfileName();
    let profile = new Cleopatra(uid, name, this);

    this.profiles.set(uid, profile);
    this.sidebar.addProfile(profile);
    this.emit("profileCreated", uid);

    return profile;
  },

  /**
   * Starts or stops profile recording.
   */
  toggleRecording: function () {
    let profile = this.recordingProfile;

    if (!profile) {
      profile = this.createProfile();

      this.startProfiling(profile.name, () => {
        profile.isStarted = true;

        this.sidebar.setProfileState(profile, PROFILE_RUNNING);
        this.recordingProfile = profile;
        this.emit("started");
      });

      return;
    }

    this.stopProfiling(profile.name, (data) => {
      profile.isStarted = false;
      profile.isFinished = true;
      profile.data = data;
      profile.parse(data, () => this.emit("parsed"));

      this.sidebar.setProfileState(profile, PROFILE_COMPLETED);
      this.activeProfile = profile;
      this.sidebar.selectedItem = this.sidebar.getItemByProfile(profile);
      this.recordingProfile = null;
      this.emit("stopped");
    });
  },

  /**
   * Start collecting profile data.
   *
   * @param function onStart
   *   A function to call once we get the message
   *   that profiling had been successfuly started.
   */
  startProfiling: function (name, onStart) {
    this.controller.start(name, (err) => {
      if (err) {
        return void Cu.reportError("ProfilerController.start: " + err.message);
      }

      onStart();
      this.emit("started");
    });
  },

  /**
   * Stop collecting profile data.
   *
   * @param function onStop
   *   A function to call once we get the message
   *   that profiling had been successfuly stopped.
   */
  stopProfiling: function (name, onStop) {
    this.controller.isActive((err, isActive) => {
      if (err) {
        Cu.reportError("ProfilerController.isActive: " + err.message);
        return;
      }

      if (!isActive) {
        return;
      }

      this.controller.stop(name, (err, data) => {
        if (err) {
          Cu.reportError("ProfilerController.stop: " + err.message);
          return;
        }

        onStop(data);
        this.emit("stopped", data);
      });
    });
  },

  /**
   * Lookup an individual profile by its name.
   *
   * @param string name name of the profile
   * @return profile object or null
   */
  getProfileByName: function PP_getProfileByName(name) {
    if (!this.profiles) {
      return null;
    }

    for (let [ uid, profile ] of this.profiles) {
      if (profile.name === name) {
        return profile;
      }
    }

    return null;
  },

  /**
   * Lookup an individual profile by its UID.
   *
   * @param number uid UID of the profile
   * @return profile object or null
   */
  getProfileByUID: function PP_getProfileByUID(uid) {
    if (!this.profiles) {
      return null;
    }

    return this.profiles.get(uid) || null;
  },

  /**
   * Iterates over each available profile and calls
   * a callback with it as a parameter.
   *
   * @param function cb a callback to call
   */
  eachProfile: function PP_eachProfile(cb) {
    let uid = this._uid;

    if (!this.profiles) {
      return;
    }

    while (uid >= 0) {
      if (this.profiles.has(uid)) {
        cb(this.profiles.get(uid));
      }

      uid -= 1;
    }
  },

  /**
   * Broadcast messages to all Cleopatra instances.
   *
   * @param number target
   *   UID of the recepient profile. All profiles will receive the message
   *   but the profile specified by 'target' will have a special property,
   *   isCurrent, set to true.
   * @param object data
   *   An object with a property 'task' that will be sent over to Cleopatra.
   */
  broadcast: function PP_broadcast(target, data) {
    if (!this.profiles) {
      return;
    }

    this.eachProfile((profile) => {
      profile.message({
        uid: target,
        isCurrent: target === profile.uid,
        task: data.task
      });
    });
  },

  /**
   * Open file specified in data in either a debugger or view-source.
   *
   * @param object data
   *   An object describing the file. It must have three properties:
   *    - uri
   *    - line
   *    - isChrome (chrome files are opened via view-source)
   */
  displaySource: function PP_displaySource(data, onOpen=function() {}) {
    let win = this.window;
    let panelWin, timeout;

    function onSourceShown(event) {
      if (event.detail.url !== data.uri) {
        return;
      }

      panelWin.removeEventListener("Debugger:SourceShown", onSourceShown, false);
      panelWin.editor.setCaretPosition(data.line - 1);
      onOpen();
    }

    if (data.isChrome) {
      return void this.browserWindow.gViewSourceUtils.
        viewSource(data.uri, null, this.document, data.line);
    }

    gDevTools.showToolbox(this.target, "jsdebugger").then(function (toolbox) {
      let dbg = toolbox.getCurrentPanel();
      panelWin = dbg.panelWin;

      let view = dbg.panelWin.DebuggerView;
      if (view.Sources.selectedValue === data.uri) {
        view.editor.setCaretPosition(data.line - 1);
        onOpen();
        return;
      }

      panelWin.addEventListener("Debugger:SourceShown", onSourceShown, false);
      panelWin.DebuggerView.Sources.preferredSource = data.uri;
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

module.exports = ProfilerPanel;