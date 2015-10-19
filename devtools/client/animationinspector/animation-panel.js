/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals AnimationsController, document, performance, promise,
   gToolbox, gInspector, requestAnimationFrame, cancelAnimationFrame, L10N */

"use strict";

const {AnimationsTimeline} = require("devtools/client/animationinspector/components");
const {formatStopwatchTime} = require("devtools/client/animationinspector/utils");

var $ = (selector, target = document) => target.querySelector(selector);

/**
 * The main animations panel UI.
 */
var AnimationsPanel = {
  UI_UPDATED_EVENT: "ui-updated",
  PANEL_INITIALIZED: "panel-initialized",

  initialize: Task.async(function*() {
    if (AnimationsController.destroyed) {
      console.warn("Could not initialize the animation-panel, controller " +
                   "was destroyed");
      return;
    }
    if (this.initialized) {
      yield this.initialized.promise;
      return;
    }
    this.initialized = promise.defer();

    this.playersEl = $("#players");
    this.errorMessageEl = $("#error-message");
    this.pickerButtonEl = $("#element-picker");
    this.toggleAllButtonEl = $("#toggle-all");
    this.playTimelineButtonEl = $("#pause-resume-timeline");
    this.rewindTimelineButtonEl = $("#rewind-timeline");
    this.timelineCurrentTimeEl = $("#timeline-current-time");

    // If the server doesn't support toggling all animations at once, hide the
    // whole global toolbar.
    if (!AnimationsController.traits.hasToggleAll) {
      $("#global-toolbar").style.display = "none";
    }

    // Binding functions that need to be called in scope.
    for (let functionName of ["onPickerStarted", "onPickerStopped",
      "refreshAnimationsUI", "toggleAll", "onTabNavigated",
      "onTimelineDataChanged", "playPauseTimeline", "rewindTimeline"]) {
      this[functionName] = this[functionName].bind(this);
    }
    let hUtils = gToolbox.highlighterUtils;
    this.togglePicker = hUtils.togglePicker.bind(hUtils);

    this.animationsTimelineComponent = new AnimationsTimeline(gInspector);
    this.animationsTimelineComponent.init(this.playersEl);

    this.startListeners();

    yield this.refreshAnimationsUI();

    this.initialized.resolve();

    this.emit(this.PANEL_INITIALIZED);
  }),

  destroy: Task.async(function*() {
    if (!this.initialized) {
      return;
    }

    if (this.destroyed) {
      yield this.destroyed.promise;
      return;
    }
    this.destroyed = promise.defer();

    this.stopListeners();

    this.animationsTimelineComponent.destroy();
    this.animationsTimelineComponent = null;

    this.playersEl = this.errorMessageEl = null;
    this.toggleAllButtonEl = this.pickerButtonEl = null;
    this.playTimelineButtonEl = this.rewindTimelineButtonEl = null;
    this.timelineCurrentTimeEl = null;

    this.destroyed.resolve();
  }),

  startListeners: function() {
    AnimationsController.on(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.refreshAnimationsUI);

    this.pickerButtonEl.addEventListener("click", this.togglePicker);
    gToolbox.on("picker-started", this.onPickerStarted);
    gToolbox.on("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.addEventListener("click", this.toggleAll);
    this.playTimelineButtonEl.addEventListener("click", this.playPauseTimeline);
    this.rewindTimelineButtonEl.addEventListener("click", this.rewindTimeline);

    gToolbox.target.on("navigate", this.onTabNavigated);

    this.animationsTimelineComponent.on("timeline-data-changed",
      this.onTimelineDataChanged);
  },

  stopListeners: function() {
    AnimationsController.off(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.refreshAnimationsUI);

    this.pickerButtonEl.removeEventListener("click", this.togglePicker);
    gToolbox.off("picker-started", this.onPickerStarted);
    gToolbox.off("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.removeEventListener("click", this.toggleAll);
    this.playTimelineButtonEl.removeEventListener("click", this.playPauseTimeline);
    this.rewindTimelineButtonEl.removeEventListener("click", this.rewindTimeline);

    gToolbox.target.off("navigate", this.onTabNavigated);

    this.animationsTimelineComponent.off("timeline-data-changed",
      this.onTimelineDataChanged);
  },

  togglePlayers: function(isVisible) {
    if (isVisible) {
      document.body.removeAttribute("empty");
      document.body.setAttribute("timeline", "true");
    } else {
      document.body.setAttribute("empty", "true");
      document.body.removeAttribute("timeline");
    }
  },

  onPickerStarted: function() {
    this.pickerButtonEl.setAttribute("checked", "true");
  },

  onPickerStopped: function() {
    this.pickerButtonEl.removeAttribute("checked");
  },

  toggleAll: Task.async(function*() {
    this.toggleAllButtonEl.classList.toggle("paused");
    yield AnimationsController.toggleAll();
  }),

  /**
   * Depending on the state of the timeline either pause or play the animations
   * displayed in it.
   * If the animations are finished, this will play them from the start again.
   * If the animations are playing, this will pause them.
   * If the animations are paused, this will resume them.
   */
  playPauseTimeline: function() {
    AnimationsController.toggleCurrentAnimations(this.timelineData.isMoving)
                        .then(() => this.refreshAnimationsStateAndUI())
                        .catch(e => console.error(e));
  },

  /**
   * Reset the startTime of all current animations shown in the timeline and
   * pause them.
   */
  rewindTimeline: function() {
    AnimationsController.setCurrentTimeAll(0, true)
                        .then(() => this.refreshAnimationsStateAndUI())
                        .catch(e => console.error(e));
  },

  onTabNavigated: function() {
    this.toggleAllButtonEl.classList.remove("paused");
  },

  onTimelineDataChanged: function(e, data) {
    this.timelineData = data;
    let {isMoving, isUserDrag, time} = data;

    this.playTimelineButtonEl.classList.toggle("paused", !isMoving);

    // If the timeline data changed as a result of the user dragging the
    // scrubber, then pause all animations and set their currentTimes.
    // (Note that we want server-side requests to be sequenced, so we only do
    // this after the previous currentTime setting was done).
    if (isUserDrag && !this.setCurrentTimeAllPromise) {
      this.setCurrentTimeAllPromise =
        AnimationsController.setCurrentTimeAll(time, true)
                            .catch(error => console.error(error))
                            .then(() => this.setCurrentTimeAllPromise = null);
    }

    this.displayTimelineCurrentTime();
  },

  displayTimelineCurrentTime: function() {
    let {isMoving, isPaused, time} = this.timelineData;

    if (isMoving || isPaused) {
      this.timelineCurrentTimeEl.textContent = formatStopwatchTime(time);
    }
  },

  /**
   * Make sure all known animations have their states up to date (which is
   * useful after the playState or currentTime has been changed and in case the
   * animations aren't auto-refreshing), and then refresh the UI.
   */
  refreshAnimationsStateAndUI: Task.async(function*() {
    for (let player of AnimationsController.animationPlayers) {
      yield player.refreshState();
    }
    yield this.refreshAnimationsUI();
  }),

  /**
   * Refresh the list of animations UI. This will empty the panel and re-render
   * the various components again.
   */
  refreshAnimationsUI: Task.async(function*() {
    let done = gInspector.updating("animationspanel");

    // Empty the whole panel first.
    this.togglePlayers(true);

    // Re-render the timeline component.
    this.animationsTimelineComponent.render(
      AnimationsController.animationPlayers,
      AnimationsController.documentCurrentTime);

    // If there are no players to show, show the error message instead and
    // return.
    if (!AnimationsController.animationPlayers.length) {
      this.togglePlayers(false);
      this.emit(this.UI_UPDATED_EVENT);
      done();
      return;
    }

    this.emit(this.UI_UPDATED_EVENT);
    done();
  })
};

EventEmitter.decorate(AnimationsPanel);
