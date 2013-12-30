/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ProgressReporter
 *
 * This class is used by long-winded tasks to report progress to observers.
 * If a task has subtasks that want to report their own progress, these
 * subtasks can have their own progress reporters which are hooked up to the
 * parent progress reporter, resulting in a tree structure. A parent progress
 * reporter will calculate its progress value as a weighted sum of its
 * subreporters' progress values.
 *
 * A progress reporter has a state, an action, and a progress value.
 *
 *  - state is one of STATE_WAITING, STATE_DOING and STATE_FINISHED.
 *  - action is a string that describes the current task.
 *  - progress is the progress value as a number between 0 and 1, or NaN if
 *    indeterminate.
 *
 * A progress reporter starts out in the WAITING state. The DOING state is
 * entered with the begin method which also sets the action. While the task is
 * executing, the progress value can be updated with the setProgress method.
 * When a task has finished, it can call the finish method which is just a
 * shorthand for setProgress(1); this will set the state to FINISHED.
 *
 * Progress observers can be added with the addListener method which takes a
 * function callback. Whenever the progress value or state change, all
 * listener callbacks will be called with the progress reporter object. The
 * observer can get state, progress value and action by calling the getter
 * methods getState(), getProgress() and getAction().
 *
 * Creating child progress reporters for subtasks can be done with the
 * addSubreporter(s) methods. If a progress reporter has subreporters, normal
 * progress report functions (setProgress and finish) can no longer be called.
 * Instead, the parent reporter will listen to progress changes on its
 * subreporters and update its state automatically, and then notify its own
 * listeners.
 * When adding a subreporter, you are expected to provide an estimated
 * duration for the subtask. This value will be used as a weight when
 * calculating the progress of the parent reporter.
 */

"use strict";

const gDebugExpectedDurations = false;

function ProgressReporter() {
  this._observers = [];
  this._subreporters = [];
  this._subreporterExpectedDurationsSum = 0;
  this._progress = 0;
  this._state = ProgressReporter.STATE_WAITING;
  this._action = "";
}

ProgressReporter.STATE_WAITING = 0;
ProgressReporter.STATE_DOING = 1;
ProgressReporter.STATE_FINISHED = 2;

ProgressReporter.prototype = {
  getProgress: function () {
    return this._progress;
  },
  getState: function () {
    return this._state;
  },
  setAction: function (action) {
    this._action = action;
    this._reportProgress();
  },
  getAction: function () {
    switch (this._state) {
      case ProgressReporter.STATE_WAITING:
        return "Waiting for preceding tasks to finish...";
      case ProgressReporter.STATE_DOING:
        return this._action;
      case ProgressReporter.STATE_FINISHED:
        return "Finished.";
      default:
        throw "Broken state";
    }
  },
  addListener: function (callback) {
    this._observers.push(callback);
  },
  addSubreporter: function (expectedDuration) {
    this._subreporterExpectedDurationsSum += expectedDuration;
    var subreporter = new ProgressReporter();
    var self = this;
    subreporter.addListener(function (progress) {
      self._recalculateProgressFromSubreporters();
      self._recalculateStateAndActionFromSubreporters();
      self._reportProgress();
    });
    this._subreporters.push({ expectedDuration: expectedDuration, reporter: subreporter });
    return subreporter;
  },
  addSubreporters: function (expectedDurations) {
    var reporters = {};
    for (var key in expectedDurations) {
      reporters[key] = this.addSubreporter(expectedDurations[key]);
    }
    return reporters;
  },
  begin: function (action) {
    this._startTime = Date.now();
    this._state = ProgressReporter.STATE_DOING;
    this._action = action;
    this._reportProgress();
  },
  setProgress: function (progress) {
    if (this._subreporters.length > 0)
      throw "Can't call setProgress on a progress reporter with subreporters";
    if (progress != this._progress &&
        (progress == 1 ||
         (isNaN(progress) != isNaN(this._progress)) ||
         (progress - this._progress >= 0.01))) {
      this._progress = progress;
      if (progress == 1)
        this._transitionToFinished();
      this._reportProgress();
    }
  },
  finish: function () {
    this.setProgress(1);
  },
  _recalculateProgressFromSubreporters: function () {
    if (this._subreporters.length == 0)
      throw "Can't _recalculateProgressFromSubreporters on a progress reporter without any subreporters";
    this._progress = 0;
    for (var i = 0; i < this._subreporters.length; i++) {
      var expectedDuration = this._subreporters[i].expectedDuration;
      var reporter = this._subreporters[i].reporter;
      this._progress += reporter.getProgress() * expectedDuration / this._subreporterExpectedDurationsSum;
    }
  },
  _recalculateStateAndActionFromSubreporters: function () {
    if (this._subreporters.length == 0)
      throw "Can't _recalculateStateAndActionFromSubreporters on a progress reporter without any subreporters";
    var actions = [];
    var allWaiting = true;
    var allFinished = true;
    for (var i = 0; i < this._subreporters.length; i++) {
      var expectedDuration = this._subreporters[i].expectedDuration;
      var reporter = this._subreporters[i].reporter;
      var state = reporter.getState();
      if (state != ProgressReporter.STATE_WAITING)
        allWaiting = false;
      if (state != ProgressReporter.STATE_FINISHED)
        allFinished = false;
      if (state == ProgressReporter.STATE_DOING)
        actions.push(reporter.getAction());
    }
    if (allFinished) {
      this._transitionToFinished();
    } else if (!allWaiting) {
      this._state = ProgressReporter.STATE_DOING;
      if (actions.length == 0) {
        this._action = "About to start next task..."
      } else {
        this._action = actions.join("\n");
      }
    }
  },
  _transitionToFinished: function () {
    this._state = ProgressReporter.STATE_FINISHED;

    if (gDebugExpectedDurations) {
      this._realDuration = Date.now() - this._startTime;
      if (this._subreporters.length) {
        for (var i = 0; i < this._subreporters.length; i++) {
          var expectedDuration = this._subreporters[i].expectedDuration;
          var reporter = this._subreporters[i].reporter;
          var realDuration = reporter._realDuration;
          dump("For reporter with expectedDuration " + expectedDuration + ", real duration was " + realDuration + "\n");
        }
      }
    }
  },
  _reportProgress: function () {
    for (var i = 0; i < this._observers.length; i++) {
      this._observers[i](this);
    }
  },
};
