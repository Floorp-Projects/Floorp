/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from animation-controller.js */
/* globals document */

"use strict";

const {AnimationsTimeline} = require("devtools/client/inspector/animation-old/components/animation-timeline");
const {RateSelector} = require("devtools/client/inspector/animation-old/components/rate-selector");
const {formatStopwatchTime} = require("devtools/client/inspector/animation-old/utils");
const {KeyCodes} = require("devtools/client/shared/keycodes");

var $ = (selector, target = document) => target.querySelector(selector);

/**
 * The main animations panel UI.
 */
var AnimationsPanel = {
  UI_UPDATED_EVENT: "ui-updated",
  PANEL_INITIALIZED: "panel-initialized",

  async initialize() {
    if (AnimationsController.destroyed) {
      console.warn("Could not initialize the animation-panel, controller " +
                   "was destroyed");
      return;
    }
    if (this.initialized) {
      await this.initialized;
      return;
    }

    let resolver;
    this.initialized = new Promise(resolve => {
      resolver = resolve;
    });

    this.playersEl = $("#players");
    this.errorMessageEl = $("#error-message");
    this.pickerButtonEl = $("#element-picker");
    this.toggleAllButtonEl = $("#toggle-all");
    this.playTimelineButtonEl = $("#pause-resume-timeline");
    this.rewindTimelineButtonEl = $("#rewind-timeline");
    this.timelineCurrentTimeEl = $("#timeline-current-time");
    this.rateSelectorEl = $("#timeline-rate");

    this.rewindTimelineButtonEl.setAttribute("title",
      L10N.getStr("timeline.rewindButtonTooltip"));

    $("#all-animations-label").textContent = L10N.getStr("panel.allAnimations");

    // If the server doesn't support toggling all animations at once, hide the
    // whole global toolbar.
    if (!AnimationsController.traits.hasToggleAll) {
      $("#global-toolbar").style.display = "none";
    }

    // Binding functions that need to be called in scope.
    for (let functionName of [
      "onKeyDown", "onPickerStarted",
      "onPickerStopped", "refreshAnimationsUI", "onToggleAllClicked",
      "onTabNavigated", "onTimelineDataChanged", "onTimelinePlayClicked",
      "onTimelineRewindClicked", "onRateChanged"]) {
      this[functionName] = this[functionName].bind(this);
    }
    let hUtils = gToolbox.highlighterUtils;
    this.togglePicker = hUtils.togglePicker.bind(hUtils);

    this.animationsTimelineComponent = new AnimationsTimeline(gInspector,
      AnimationsController.traits);
    this.animationsTimelineComponent.init(this.playersEl);

    if (AnimationsController.traits.hasSetPlaybackRate) {
      this.rateSelectorComponent = new RateSelector();
      this.rateSelectorComponent.init(this.rateSelectorEl);
    }

    this.startListeners();

    await this.refreshAnimationsUI();

    resolver();
    this.emit(this.PANEL_INITIALIZED);
  },

  async destroy() {
    if (!this.initialized) {
      return;
    }

    if (this.destroyed) {
      await this.destroyed;
      return;
    }

    let resolver;
    this.destroyed = new Promise(resolve => {
      resolver = resolve;
    });

    this.stopListeners();

    this.animationsTimelineComponent.destroy();
    this.animationsTimelineComponent = null;

    if (this.rateSelectorComponent) {
      this.rateSelectorComponent.destroy();
      this.rateSelectorComponent = null;
    }

    this.playersEl = this.errorMessageEl = null;
    this.toggleAllButtonEl = this.pickerButtonEl = null;
    this.playTimelineButtonEl = this.rewindTimelineButtonEl = null;
    this.timelineCurrentTimeEl = this.rateSelectorEl = null;

    resolver();
  },

  startListeners: function() {
    AnimationsController.on(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.refreshAnimationsUI);

    this.pickerButtonEl.addEventListener("click", this.togglePicker);
    gToolbox.on("picker-started", this.onPickerStarted);
    gToolbox.on("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.addEventListener("click", this.onToggleAllClicked);
    this.playTimelineButtonEl.addEventListener(
      "click", this.onTimelinePlayClicked);
    this.rewindTimelineButtonEl.addEventListener(
      "click", this.onTimelineRewindClicked);

    document.addEventListener("keydown", this.onKeyDown);

    gToolbox.target.on("navigate", this.onTabNavigated);

    this.animationsTimelineComponent.on("timeline-data-changed",
      this.onTimelineDataChanged);

    if (this.rateSelectorComponent) {
      this.rateSelectorComponent.on("rate-changed", this.onRateChanged);
    }
  },

  stopListeners: function() {
    AnimationsController.off(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.refreshAnimationsUI);

    this.pickerButtonEl.removeEventListener("click", this.togglePicker);
    gToolbox.off("picker-started", this.onPickerStarted);
    gToolbox.off("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.removeEventListener("click",
      this.onToggleAllClicked);
    this.playTimelineButtonEl.removeEventListener("click",
      this.onTimelinePlayClicked);
    this.rewindTimelineButtonEl.removeEventListener("click",
      this.onTimelineRewindClicked);

    document.removeEventListener("keydown", this.onKeyDown);

    gToolbox.target.off("navigate", this.onTabNavigated);

    this.animationsTimelineComponent.off("timeline-data-changed",
      this.onTimelineDataChanged);

    if (this.rateSelectorComponent) {
      this.rateSelectorComponent.off("rate-changed", this.onRateChanged);
    }
  },

  onKeyDown: function(event) {
    // If the space key is pressed, it should toggle the play state of
    // the animations displayed in the panel, or of all the animations on
    // the page if the selected node does not have any animation on it.
    if (event.keyCode === KeyCodes.DOM_VK_SPACE) {
      if (AnimationsController.animationPlayers.length > 0) {
        this.playPauseTimeline().catch(console.error);
      } else {
        this.toggleAll().catch(console.error);
      }
      event.preventDefault();
    }
  },

  togglePlayers: function(isVisible) {
    if (isVisible) {
      document.body.removeAttribute("empty");
      document.body.setAttribute("timeline", "true");
    } else {
      document.body.setAttribute("empty", "true");
      document.body.removeAttribute("timeline");
      $("#error-type").textContent = L10N.getStr("panel.invalidElementSelected");
      $("#error-hint").textContent = L10N.getStr("panel.selectElement");
    }
  },

  onPickerStarted: function() {
    this.pickerButtonEl.classList.add("checked");
  },

  onPickerStopped: function() {
    this.pickerButtonEl.classList.remove("checked");
  },

  onToggleAllClicked: function() {
    this.toggleAll().catch(console.error);
  },

  /**
   * Toggle (pause/play) all animations in the current target
   * and update the UI the toggleAll button.
   */
  async toggleAll() {
    this.toggleAllButtonEl.classList.toggle("paused");
    await AnimationsController.toggleAll();
  },

  onTimelinePlayClicked: function() {
    this.playPauseTimeline().catch(console.error);
  },

  /**
   * Depending on the state of the timeline either pause or play the animations
   * displayed in it.
   * If the animations are finished, this will play them from the start again.
   * If the animations are playing, this will pause them.
   * If the animations are paused, this will resume them.
   *
   * @return {Promise} Resolves when the playState is changed and the UI
   * is refreshed
   */
  playPauseTimeline: function() {
    return AnimationsController
      .toggleCurrentAnimations(this.timelineData.isMoving)
      .then(() => this.refreshAnimationsStateAndUI());
  },

  onTimelineRewindClicked: function() {
    this.rewindTimeline().catch(console.error);
  },

  /**
   * Reset the startTime of all current animations shown in the timeline and
   * pause them.
   *
   * @return {Promise} Resolves when currentTime is set and the UI is refreshed
   */
  rewindTimeline: function() {
    return AnimationsController
      .setCurrentTimeAll(0, true)
      .then(() => this.refreshAnimationsStateAndUI());
  },

  /**
   * Set the playback rate of all current animations shown in the timeline to
   * the value of this.rateSelectorEl.
   */
  onRateChanged: function(rate) {
    AnimationsController.setPlaybackRateAll(rate)
                        .then(() => this.refreshAnimationsStateAndUI())
                        .catch(console.error);
  },

  onTabNavigated: function() {
    this.toggleAllButtonEl.classList.remove("paused");
  },

  onTimelineDataChanged: function(data) {
    this.timelineData = data;
    let {isMoving, isUserDrag, time} = data;

    this.playTimelineButtonEl.classList.toggle("paused", !isMoving);

    let l10nPlayProperty = isMoving ? "timeline.resumedButtonTooltip" :
                                      "timeline.pausedButtonTooltip";

    this.playTimelineButtonEl.setAttribute("title",
      L10N.getStr(l10nPlayProperty));

    // If the timeline data changed as a result of the user dragging the
    // scrubber, then pause all animations and set their currentTimes.
    // (Note that we want server-side requests to be sequenced, so we only do
    // this after the previous currentTime setting was done).
    if (isUserDrag && !this.setCurrentTimeAllPromise) {
      this.setCurrentTimeAllPromise =
        AnimationsController.setCurrentTimeAll(time, true)
                            .catch(console.error)
                            .then(() => {
                              this.setCurrentTimeAllPromise = null;
                            });
    }

    this.displayTimelineCurrentTime();
  },

  displayTimelineCurrentTime: function() {
    let {time} = this.timelineData;
    this.timelineCurrentTimeEl.textContent = formatStopwatchTime(time);
  },

  /**
   * Make sure all known animations have their states up to date (which is
   * useful after the playState or currentTime has been changed and in case the
   * animations aren't auto-refreshing), and then refresh the UI.
   */
  async refreshAnimationsStateAndUI() {
    for (let player of AnimationsController.animationPlayers) {
      await player.refreshState();
    }
    await this.refreshAnimationsUI();
  },

  /**
   * Refresh the list of animations UI. This will empty the panel and re-render
   * the various components again.
   */
  async refreshAnimationsUI() {
    // Empty the whole panel first.
    this.togglePlayers(true);

    // Re-render the timeline component.
    this.animationsTimelineComponent.render(
      AnimationsController.animationPlayers,
      AnimationsController.documentCurrentTime);

    // Re-render the rate selector component.
    if (this.rateSelectorComponent) {
      this.rateSelectorComponent.render(AnimationsController.animationPlayers);
    }

    // If there are no players to show, show the error message instead and
    // return.
    if (!AnimationsController.animationPlayers.length) {
      this.togglePlayers(false);
      this.emit(this.UI_UPDATED_EVENT);
      return;
    }

    this.emit(this.UI_UPDATED_EVENT);
  }
};

EventEmitter.decorate(AnimationsPanel);
