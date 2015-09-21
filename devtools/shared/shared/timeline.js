/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Many Gecko operations (painting, reflows, restyle, ...) can be tracked
 * in real time. A marker is a representation of one operation. A marker
 * has a name, start and end timestamps. Markers are stored in docShells.
 *
 * This module exposes this tracking mechanism. To use with devtools' RDP,
 * use toolkit/devtools/server/actors/timeline.js directly.
 *
 * To start/stop recording markers:
 *   timeline.start()
 *   timeline.stop()
 *   timeline.isRecording()
 *
 * When markers are available, an event is emitted:
 *   timeline.on("markers", function(markers) {...})
 */

const { Ci, Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
// Be aggressive about lazy loading, as this will run on every
// toolbox startup
loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "Timers", "sdk/timers");
loader.lazyRequireGetter(this, "Task", "resource://gre/modules/Task.jsm", true);
loader.lazyRequireGetter(this, "Memory", "devtools/shared/shared/memory", true);
loader.lazyRequireGetter(this, "Framerate", "devtools/shared/shared/framerate", true);
loader.lazyRequireGetter(this, "StackFrameCache", "devtools/server/actors/utils/stack", true);
loader.lazyRequireGetter(this, "EventTarget", "sdk/event/target", true);

// How often do we pull markers from the docShells, and therefore, how often do
// we send events to the front (knowing that when there are no markers in the
// docShell, no event is sent).
const DEFAULT_TIMELINE_DATA_PULL_TIMEOUT = 200; // ms

/**
 * The timeline actor pops and forwards timeline markers registered in docshells.
 */
var Timeline = exports.Timeline = Class({
  extends: EventTarget,

  /**
   * Initializes this actor with the provided connection and tab actor.
   */
  initialize: function (tabActor) {
    this.tabActor = tabActor;

    this._isRecording = false;
    this._stackFrames = null;
    this._memory = null;
    this._framerate = null;

    // Make sure to get markers from new windows as they become available
    this._onWindowReady = this._onWindowReady.bind(this);
    this._onGarbageCollection = this._onGarbageCollection.bind(this);
    events.on(this.tabActor, "window-ready", this._onWindowReady);
  },

  /**
   * Destroys this actor, stopping recording first.
   */
  destroy: function() {
    this.stop();

    events.off(this.tabActor, "window-ready", this._onWindowReady);
    this.tabActor = null;

    if (this._memory) {
      this._memory.destroy();
      this._memory = null;
    }

    if (this._framerate) {
      this._framerate.destroy();
      this._framerate = null;
    }
  },

  /**
   * Get the list of docShells in the currently attached tabActor. Note that we
   * always list the docShells included in the real root docShell, even if the
   * tabActor was switched to a child frame. This is because for now, paint
   * markers are only recorded at parent frame level so switching the timeline
   * to a child frame would hide all paint markers.
   * See https://bugzilla.mozilla.org/show_bug.cgi?id=1050773#c14
   * @return {Array}
   */
  get docShells() {
    let originalDocShell;

    if (this.tabActor.isRootActor) {
      originalDocShell = this.tabActor.docShell;
    } else {
      originalDocShell = this.tabActor.originalDocShell;
    }

    let docShellsEnum = originalDocShell.getDocShellEnumerator(
      Ci.nsIDocShellTreeItem.typeAll,
      Ci.nsIDocShell.ENUMERATE_FORWARDS
    );

    let docShells = [];
    while (docShellsEnum.hasMoreElements()) {
      let docShell = docShellsEnum.getNext();
      docShells.push(docShell.QueryInterface(Ci.nsIDocShell));
    }

    return docShells;
  },

  /**
   * At regular intervals, pop the markers from the docshell, and forward
   * markers, memory, tick and frames events, if any.
   */
  _pullTimelineData: function() {
    if (!this._isRecording || !this.docShells.length) {
      return;
    }

    let endTime = this.docShells[0].now();
    let markers = [];

    for (let docShell of this.docShells) {
      markers.push(...docShell.popProfileTimelineMarkers());
    }

    // The docshell may return markers with stack traces attached.
    // Here we transform the stack traces via the stack frame cache,
    // which lets us preserve tail sharing when transferring the
    // frames to the client.  We must waive xrays here because Firefox
    // doesn't understand that the Debugger.Frame object is safe to
    // use from chrome.  See Tutorial-Alloc-Log-Tree.md.
    for (let marker of markers) {
      if (marker.stack) {
        marker.stack = this._stackFrames.addFrame(Cu.waiveXrays(marker.stack));
      }
      if (marker.endStack) {
        marker.endStack = this._stackFrames.addFrame(Cu.waiveXrays(marker.endStack));
      }
    }

    let frames = this._stackFrames.makeEvent();
    if (frames) {
      events.emit(this, "frames", endTime, frames);
    }
    if (markers.length > 0) {
      events.emit(this, "markers", markers, endTime);
    }
    if (this._withMemory) {
      events.emit(this, "memory", endTime, this._memory.measure());
    }
    if (this._withTicks) {
      events.emit(this, "ticks", endTime, this._framerate.getPendingTicks());
    }

    this._dataPullTimeout = Timers.setTimeout(() => {
      this._pullTimelineData();
    }, DEFAULT_TIMELINE_DATA_PULL_TIMEOUT);
  },

  /**
   * Are we recording profile markers currently?
   */
  isRecording: function () {
    return this._isRecording;
  },

  /**
   * Start recording profile markers.
   *
   * @option {boolean} withMemory
   *         Boolean indiciating whether we want memory measurements sampled. A memory actor
   *         will be created regardless (to hook into GC events), but this determines
   *         whether or not a `memory` event gets fired.
   * @option {boolean} withTicks
   *         Boolean indicating whether a `ticks` event is fired and a FramerateActor
   *         is created.
   */
  start: Task.async(function *({ withMemory, withTicks }) {
    let startTime = this._startTime = this.docShells[0].now();

    if (this._isRecording) {
      return startTime;
    }

    this._isRecording = true;
    this._stackFrames = new StackFrameCache();
    this._stackFrames.initFrames();
    this._withMemory = withMemory;
    this._withTicks = withTicks;

    for (let docShell of this.docShells) {
      docShell.recordProfileTimelineMarkers = true;
    }

    this._memory = new Memory(this.tabActor, this._stackFrames);
    this._memory.attach();
    events.on(this._memory, "garbage-collection", this._onGarbageCollection);

    if (withTicks) {
      this._framerate = new Framerate(this.tabActor);
      this._framerate.startRecording();
    }

    this._pullTimelineData();
    return startTime;
  }),

  /**
   * Stop recording profile markers.
   */
  stop: Task.async(function *() {
    if (!this._isRecording) {
      return;
    }
    this._isRecording = false;
    this._stackFrames = null;

    events.off(this._memory, "garbage-collection", this._onGarbageCollection);
    this._memory.detach();

    if (this._framerate) {
      this._framerate.stopRecording();
      this._framerate = null;
    }

    for (let docShell of this.docShells) {
      docShell.recordProfileTimelineMarkers = false;
    }

    Timers.clearTimeout(this._dataPullTimeout);
    return this.docShells[0].now();
  }),

  /**
   * When a new window becomes available in the tabActor, start recording its
   * markers if we were recording.
   */
  _onWindowReady: function({ window }) {
    if (this._isRecording) {
      let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell);
      docShell.recordProfileTimelineMarkers = true;
    }
  },

  /**
   * Fired when the Memory component emits a `garbage-collection` event. Used to
   * take the data and make it look like the rest of our markers.
   *
   * A GC "marker" here represents a full GC cycle, which may contain several incremental
   * events within its `collection` array. The marker contains a `reason` field, indicating
   * why there was a GC, and may contain a `nonincrementalReason` when SpiderMonkey could
   * not incrementally collect garbage.
   */
  _onGarbageCollection: function ({ collections, gcCycleNumber, reason, nonincrementalReason }) {
    if (!this._isRecording || !this.docShells.length) {
      return;
    }

    let endTime = this.docShells[0].now();

    events.emit(this, "markers", collections.map(({ startTimestamp: start, endTimestamp: end }) => {
      return {
        name: "GarbageCollection",
        causeName: reason,
        nonincrementalReason: nonincrementalReason,
        cycle: gcCycleNumber,
        start,
        end,
      };
    }), endTime);
  },
});
