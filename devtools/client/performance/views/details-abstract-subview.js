/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ../performance-controller.js */
/* import-globals-from ../performance-view.js */
/* exported DetailsSubview */
"use strict";

/**
 * A base class from which all detail views inherit.
 */
var DetailsSubview = {
  /**
   * Sets up the view with event binding.
   */
  initialize: function() {
    this._onRecordingStoppedOrSelected = this._onRecordingStoppedOrSelected.bind(this);
    this._onOverviewRangeChange = this._onOverviewRangeChange.bind(this);
    this._onDetailsViewSelected = this._onDetailsViewSelected.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);

    PerformanceController.on(EVENTS.RECORDING_STATE_CHANGE,
                             this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.RECORDING_SELECTED,
                             this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.PREF_CHANGED, this._onPrefChanged);
    OverviewView.on(EVENTS.UI_OVERVIEW_RANGE_SELECTED, this._onOverviewRangeChange);
    DetailsView.on(EVENTS.UI_DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);

    const self = this;
    const originalRenderFn = this.render;
    const afterRenderFn = () => {
      this._wasRendered = true;
    };

    this.render = async function(...args) {
      const maybeRetval = await originalRenderFn.apply(self, args);
      afterRenderFn();
      return maybeRetval;
    };
  },

  /**
   * Unbinds events.
   */
  destroy: function() {
    clearNamedTimeout("range-change-debounce");

    PerformanceController.off(EVENTS.RECORDING_STATE_CHANGE,
                              this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.RECORDING_SELECTED,
                              this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.PREF_CHANGED, this._onPrefChanged);
    OverviewView.off(EVENTS.UI_OVERVIEW_RANGE_SELECTED, this._onOverviewRangeChange);
    DetailsView.off(EVENTS.UI_DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);
  },

  /**
   * Returns true if this view was rendered at least once.
   */
  get wasRenderedAtLeastOnce() {
    return !!this._wasRendered;
  },

  /**
   * Amount of time (in milliseconds) to wait until this view gets updated,
   * when the range is changed in the overview.
   */
  rangeChangeDebounceTime: 0,

  /**
   * When the overview range changes, all details views will require a
   * rerendering at a later point, determined by `shouldUpdateWhenShown` and
   * `canUpdateWhileHidden` and whether or not its the current view.
   * Set `requiresUpdateOnRangeChange` to false to not invalidate the view
   * when the range changes.
   */
  requiresUpdateOnRangeChange: true,

  /**
   * Flag specifying if this view should be updated when selected. This will
   * be set to true, for example, when the range changes in the overview and
   * this view is not currently visible.
   */
  shouldUpdateWhenShown: false,

  /**
   * Flag specifying if this view may get updated even when it's not selected.
   * Should only be used in tests.
   */
  canUpdateWhileHidden: false,

  /**
   * An array of preferences under `devtools.performance.ui.` that the view should
   * rerender and callback `this._onRerenderPrefChanged` upon change.
   */
  rerenderPrefs: [],

  /**
   * An array of preferences under `devtools.performance.` that the view should
   * observe and callback `this._onObservedPrefChange` upon change.
   */
  observedPrefs: [],

  /**
   * Flag specifying if this view should update while the overview selection
   * area is actively being dragged by the mouse.
   */
  shouldUpdateWhileMouseIsActive: false,

  /**
   * Called when recording stops or is selected.
   */
  _onRecordingStoppedOrSelected: function(state, recording) {
    if (typeof state !== "string") {
      recording = state;
    }
    if (arguments.length === 3 && state !== "recording-stopped") {
      return;
    }

    if (!recording || !recording.isCompleted()) {
      return;
    }
    if (DetailsView.isViewSelected(this) || this.canUpdateWhileHidden) {
      this.render(OverviewView.getTimeInterval());
    } else {
      this.shouldUpdateWhenShown = true;
    }
  },

  /**
   * Fired when a range is selected or cleared in the OverviewView.
   */
  _onOverviewRangeChange: function(interval) {
    if (!this.requiresUpdateOnRangeChange) {
      return;
    }
    if (DetailsView.isViewSelected(this)) {
      const debounced = () => {
        if (!this.shouldUpdateWhileMouseIsActive && OverviewView.isMouseActive) {
          // Don't render yet, while the selection is still being dragged.
          setNamedTimeout("range-change-debounce", this.rangeChangeDebounceTime,
                          debounced);
        } else {
          this.render(interval);
        }
      };
      setNamedTimeout("range-change-debounce", this.rangeChangeDebounceTime, debounced);
    } else {
      this.shouldUpdateWhenShown = true;
    }
  },

  /**
   * Fired when a view is selected in the DetailsView.
   */
  _onDetailsViewSelected: function() {
    if (DetailsView.isViewSelected(this) && this.shouldUpdateWhenShown) {
      this.render(OverviewView.getTimeInterval());
      this.shouldUpdateWhenShown = false;
    }
  },

  /**
   * Fired when a preference in `devtools.performance.ui.` is changed.
   */
  _onPrefChanged: function(prefName, prefValue) {
    if (~this.observedPrefs.indexOf(prefName) && this._onObservedPrefChange) {
      this._onObservedPrefChange(prefName);
    }

    // All detail views require a recording to be complete, so do not
    // attempt to render if recording is in progress or does not exist.
    const recording = PerformanceController.getCurrentRecording();
    if (!recording || !recording.isCompleted()) {
      return;
    }

    if (!~this.rerenderPrefs.indexOf(prefName)) {
      return;
    }

    if (this._onRerenderPrefChanged) {
      this._onRerenderPrefChanged(prefName);
    }

    if (DetailsView.isViewSelected(this) || this.canUpdateWhileHidden) {
      this.render(OverviewView.getTimeInterval());
    } else {
      this.shouldUpdateWhenShown = true;
    }
  }
};
