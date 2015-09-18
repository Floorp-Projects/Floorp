/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals AnimationsController, document, performance, promise,
   gToolbox, gInspector, requestAnimationFrame, cancelAnimationFrame, L10N */

"use strict";

const {AnimationsTimeline} = require("devtools/animationinspector/components");

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

    this.playersEl = document.querySelector("#players");
    this.errorMessageEl = document.querySelector("#error-message");
    this.pickerButtonEl = document.querySelector("#element-picker");
    this.toggleAllButtonEl = document.querySelector("#toggle-all");
    this.playTimelineButtonEl = document.querySelector("#pause-resume-timeline");

    // If the server doesn't support toggling all animations at once, hide the
    // whole global toolbar.
    if (!AnimationsController.traits.hasToggleAll) {
      document.querySelector("#global-toolbar").style.display = "none";
    }

    // Binding functions that need to be called in scope.
    for (let functionName of ["onPickerStarted", "onPickerStopped",
      "refreshAnimations", "toggleAll", "onTabNavigated",
      "onTimelineDataChanged", "playPauseTimeline"]) {
      this[functionName] = this[functionName].bind(this);
    }
    let hUtils = gToolbox.highlighterUtils;
    this.togglePicker = hUtils.togglePicker.bind(hUtils);

    this.animationsTimelineComponent = new AnimationsTimeline(gInspector);
    this.animationsTimelineComponent.init(this.playersEl);

    this.startListeners();

    yield this.refreshAnimations();

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

    yield this.destroyPlayerWidgets();

    this.playersEl = this.errorMessageEl = null;
    this.toggleAllButtonEl = this.pickerButtonEl = null;
    this.playTimelineButtonEl = null;

    this.destroyed.resolve();
  }),

  startListeners: function() {
    AnimationsController.on(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.refreshAnimations);

    this.pickerButtonEl.addEventListener("click", this.togglePicker);
    gToolbox.on("picker-started", this.onPickerStarted);
    gToolbox.on("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.addEventListener("click", this.toggleAll);
    this.playTimelineButtonEl.addEventListener("click", this.playPauseTimeline);
    gToolbox.target.on("navigate", this.onTabNavigated);

    this.animationsTimelineComponent.on("timeline-data-changed",
      this.onTimelineDataChanged);
  },

  stopListeners: function() {
    AnimationsController.off(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.refreshAnimations);

    this.pickerButtonEl.removeEventListener("click", this.togglePicker);
    gToolbox.off("picker-started", this.onPickerStarted);
    gToolbox.off("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.removeEventListener("click", this.toggleAll);
    this.playTimelineButtonEl.removeEventListener("click", this.playPauseTimeline);
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
  playPauseTimeline: Task.async(function*() {
    yield AnimationsController.toggleCurrentAnimations(this.timelineData.isMoving);

    // Now that the playState have been changed make sure the player (the
    // fronts) are up to date, and then refresh the UI.
    for (let player of AnimationsController.animationPlayers) {
      yield player.refreshState();
    }
    yield this.refreshAnimations();
  }),

  onTabNavigated: function() {
    this.toggleAllButtonEl.classList.remove("paused");
  },

  onTimelineDataChanged: function(e, data) {
    this.timelineData = data;
    let {isPaused, isMoving, time} = data;

    this.playTimelineButtonEl.classList.toggle("paused", !isMoving);

    // Pause all animations and set their currentTimes (but only do this after
    // the previous currentTime setting is done, as this gets called many times
    // when users drag the scrubber with the mouse, and we want the server-side
    // requests to be sequenced).
    if (isPaused && !this.setCurrentTimeAllPromise) {
      this.setCurrentTimeAllPromise =
        AnimationsController.setCurrentTimeAll(time, true)
                            .catch(error => console.error(error))
                            .then(() => this.setCurrentTimeAllPromise = null);
    }
  },

  refreshAnimations: Task.async(function*() {
    let done = gInspector.updating("animationspanel");

    // Empty the whole panel first.
    this.togglePlayers(true);
    yield this.destroyPlayerWidgets();

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
  }),

  destroyPlayerWidgets: Task.async(function*() {
    if (!this.playerWidgets) {
      return;
    }

    let destroyers = this.playerWidgets.map(widget => widget.destroy());
    yield promise.all(destroyers);
    this.playerWidgets = null;
    this.playersEl.innerHTML = "";
  })
};

EventEmitter.decorate(AnimationsPanel);
