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
 * use devtools/server/actors/timeline.js directly.
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

// Be aggressive about lazy loading, as this will run on every
// toolbox startup
loader.lazyRequireGetter(this, "Memory", "devtools/server/performance/memory", true);
loader.lazyRequireGetter(this, "Framerate", "devtools/server/performance/framerate", true);
loader.lazyRequireGetter(this, "StackFrameCache", "devtools/server/actors/utils/stack", true);
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

// How often do we pull markers from the docShells, and therefore, how often do
// we send events to the front (knowing that when there are no markers in the
// docShell, no event is sent). In milliseconds.
const DEFAULT_TIMELINE_DATA_PULL_TIMEOUT = 200;

/**
 * The timeline actor pops and forwards timeline markers registered in docshells.
 */
function Timeline(targetActor) {
  EventEmitter.decorate(this);

  this.targetActor = targetActor;

  this._isRecording = false;
  this._stackFrames = null;
  this._memory = null;
  this._framerate = null;

  // Make sure to get markers from new windows as they become available
  this._onWindowReady = this._onWindowReady.bind(this);
  this._onGarbageCollection = this._onGarbageCollection.bind(this);
  this.targetActor.on("window-ready", this._onWindowReady);
}

Timeline.prototype = {
  /**
   * Destroys this actor, stopping recording first.
   */
  destroy: function() {
    this.stop();

    this.targetActor.off("window-ready", this._onWindowReady);
    this.targetActor = null;
  },

  /**
   * Get the list of docShells in the currently attached targetActor. Note that
   * we always list the docShells included in the real root docShell, even if
   * the targetActor was switched to a child frame. This is because for now,
   * paint markers are only recorded at parent frame level so switching the
   * timeline to a child frame would hide all paint markers.
   * See https://bugzilla.mozilla.org/show_bug.cgi?id=1050773#c14
   * @return {Array}
   */
  get docShells() {
    let originalDocShell;
    const docShells = [];

    if (this.targetActor.isRootActor) {
      originalDocShell = this.targetActor.docShell;
    } else {
      originalDocShell = this.targetActor.originalDocShell;
    }

    if (!originalDocShell) {
      return docShells;
    }

    const docShellsEnum = originalDocShell.getDocShellEnumerator(
      Ci.nsIDocShellTreeItem.typeAll,
      Ci.nsIDocShell.ENUMERATE_FORWARDS
    );

    while (docShellsEnum.hasMoreElements()) {
      const docShell = docShellsEnum.getNext();
      docShells.push(docShell.QueryInterface(Ci.nsIDocShell));
    }

    return docShells;
  },

  /**
   * At regular intervals, pop the markers from the docshell, and forward
   * markers, memory, tick and frames events, if any.
   */
  _pullTimelineData: function() {
    const docShells = this.docShells;
    if (!this._isRecording || !docShells.length) {
      return;
    }

    const endTime = docShells[0].now();
    const markers = [];

    // Gather markers if requested.
    if (this._withMarkers || this._withDocLoadingEvents) {
      for (const docShell of docShells) {
        for (const marker of docShell.popProfileTimelineMarkers()) {
          markers.push(marker);

          // The docshell may return markers with stack traces attached.
          // Here we transform the stack traces via the stack frame cache,
          // which lets us preserve tail sharing when transferring the
          // frames to the client.  We must waive xrays here because Firefox
          // doesn't understand that the Debugger.Frame object is safe to
          // use from chrome.  See Tutorial-Alloc-Log-Tree.md.
          if (this._withFrames) {
            if (marker.stack) {
              marker.stack = this._stackFrames.addFrame(Cu.waiveXrays(marker.stack));
            }
            if (marker.endStack) {
              marker.endStack = this._stackFrames.addFrame(
                Cu.waiveXrays(marker.endStack)
              );
            }
          }

          // Emit some helper events for "DOMContentLoaded" and "Load" markers.
          if (this._withDocLoadingEvents) {
            if (marker.name == "document::DOMContentLoaded" ||
                marker.name == "document::Load") {
              this.emit("doc-loading", marker, endTime);
            }
          }
        }
      }
    }

    // Emit markers if requested.
    if (this._withMarkers && markers.length > 0) {
      this.emit("markers", markers, endTime);
    }

    // Emit framerate data if requested.
    if (this._withTicks) {
      this.emit("ticks", endTime, this._framerate.getPendingTicks());
    }

    // Emit memory data if requested.
    if (this._withMemory) {
      this.emit("memory", endTime, this._memory.measure());
    }

    // Emit stack frames data if requested.
    if (this._withFrames && this._withMarkers) {
      const frames = this._stackFrames.makeEvent();
      if (frames) {
        this.emit("frames", endTime, frames);
      }
    }

    this._dataPullTimeout = setTimeout(() => {
      this._pullTimelineData();
    }, DEFAULT_TIMELINE_DATA_PULL_TIMEOUT);
  },

  /**
   * Are we recording profile markers currently?
   */
  isRecording: function() {
    return this._isRecording;
  },

  /**
   * Start recording profile markers.
   *
   * @option {boolean} withMarkers
   *         Boolean indicating whether or not timeline markers are emitted
   *         once they're accumulated every `DEFAULT_TIMELINE_DATA_PULL_TIMEOUT`
   *         milliseconds.
   * @option {boolean} withTicks
   *         Boolean indicating whether a `ticks` event is fired and a
   *         FramerateActor is created.
   * @option {boolean} withMemory
   *         Boolean indiciating whether we want memory measurements sampled.
   * @option {boolean} withFrames
   *         Boolean indicating whether or not stack frames should be handled
   *         from timeline markers.
   * @option {boolean} withGCEvents
   *         Boolean indicating whether or not GC markers should be emitted.
   *         TODO: Remove these fake GC markers altogether in bug 1198127.
   * @option {boolean} withDocLoadingEvents
   *         Boolean indicating whether or not DOMContentLoaded and Load
   *         marker events are emitted.
   */
  async start({
    withMarkers,
    withTicks,
    withMemory,
    withFrames,
    withGCEvents,
    withDocLoadingEvents,
  }) {
    const docShells = this.docShells;
    if (!docShells.length) {
      return -1;
    }
    const startTime = this._startTime = docShells[0].now();
    if (this._isRecording) {
      return startTime;
    }

    this._isRecording = true;
    this._withMarkers = !!withMarkers;
    this._withTicks = !!withTicks;
    this._withMemory = !!withMemory;
    this._withFrames = !!withFrames;
    this._withGCEvents = !!withGCEvents;
    this._withDocLoadingEvents = !!withDocLoadingEvents;

    if (this._withMarkers || this._withDocLoadingEvents) {
      for (const docShell of docShells) {
        docShell.recordProfileTimelineMarkers = true;
      }
    }

    if (this._withTicks) {
      this._framerate = new Framerate(this.targetActor);
      this._framerate.startRecording();
    }

    if (this._withMemory || this._withGCEvents) {
      this._memory = new Memory(this.targetActor, this._stackFrames);
      this._memory.attach();
    }

    if (this._withGCEvents) {
      this._memory.on("garbage-collection", this._onGarbageCollection);
    }

    if (this._withFrames && this._withMarkers) {
      this._stackFrames = new StackFrameCache();
      this._stackFrames.initFrames();
    }

    this._pullTimelineData();
    return startTime;
  },

  /**
   * Stop recording profile markers.
   */
  async stop() {
    const docShells = this.docShells;
    if (!docShells.length) {
      return -1;
    }
    const endTime = this._startTime = docShells[0].now();
    if (!this._isRecording) {
      return endTime;
    }

    if (this._withMarkers || this._withDocLoadingEvents) {
      for (const docShell of docShells) {
        docShell.recordProfileTimelineMarkers = false;
      }
    }

    if (this._withTicks) {
      this._framerate.stopRecording();
      this._framerate.destroy();
      this._framerate = null;
    }

    if (this._withMemory || this._withGCEvents) {
      this._memory.detach();
      this._memory.destroy();
    }

    if (this._withGCEvents) {
      this._memory.off("garbage-collection", this._onGarbageCollection);
    }

    if (this._withFrames && this._withMarkers) {
      this._stackFrames = null;
    }

    this._isRecording = false;
    this._withMarkers = false;
    this._withTicks = false;
    this._withMemory = false;
    this._withFrames = false;
    this._withDocLoadingEvents = false;
    this._withGCEvents = false;

    clearTimeout(this._dataPullTimeout);

    return endTime;
  },

  /**
   * When a new window becomes available in the targetActor, start recording its
   * markers if we were recording.
   */
  _onWindowReady: function({ window }) {
    if (this._isRecording) {
      const docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
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
   * events within its `collection` array. The marker contains a `reason` field,
   * indicating why there was a GC, and may contain a `nonincrementalReason` when
   * SpiderMonkey could not incrementally collect garbage.
   */
  _onGarbageCollection: function({
    collections, gcCycleNumber, reason, nonincrementalReason
  }) {
    const docShells = this.docShells;
    if (!this._isRecording || !docShells.length) {
      return;
    }

    const endTime = docShells[0].now();

    this.emit("markers", collections.map(({
      startTimestamp: start, endTimestamp: end
    }) => {
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
};

exports.Timeline = Timeline;
