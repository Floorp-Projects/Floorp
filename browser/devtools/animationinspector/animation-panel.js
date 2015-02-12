/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The main animations panel UI.
 */
let AnimationsPanel = {
  UI_UPDATED_EVENT: "ui-updated",
  PANEL_INITIALIZED: "panel-initialized",

  initialize: Task.async(function*() {
    if (this.initialized) {
      return this.initialized.promise;
    }
    this.initialized = promise.defer();

    this.playersEl = document.querySelector("#players");
    this.errorMessageEl = document.querySelector("#error-message");
    this.pickerButtonEl = document.querySelector("#element-picker");
    this.toggleAllButtonEl = document.querySelector("#toggle-all");

    // If the server doesn't support toggling all animations at once, hide the
    // whole bottom toolbar.
    if (!AnimationsController.hasToggleAll) {
      document.querySelector("#toolbar").style.display = "none";
    }

    let hUtils = gToolbox.highlighterUtils;
    this.togglePicker = hUtils.togglePicker.bind(hUtils);
    this.onPickerStarted = this.onPickerStarted.bind(this);
    this.onPickerStopped = this.onPickerStopped.bind(this);
    this.createPlayerWidgets = this.createPlayerWidgets.bind(this);
    this.toggleAll = this.toggleAll.bind(this);
    this.onTabNavigated = this.onTabNavigated.bind(this);

    this.startListeners();

    this.initialized.resolve();

    this.emit(this.PANEL_INITIALIZED);
  }),

  destroy: Task.async(function*() {
    if (!this.initialized) {
      return;
    }

    if (this.destroyed) {
      return this.destroyed.promise;
    }
    this.destroyed = promise.defer();

    this.stopListeners();
    yield this.destroyPlayerWidgets();

    this.playersEl = this.errorMessageEl = null;
    this.toggleAllButtonEl = this.pickerButtonEl = null;

    this.destroyed.resolve();
  }),

  startListeners: function() {
    AnimationsController.on(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.createPlayerWidgets);

    this.pickerButtonEl.addEventListener("click", this.togglePicker, false);
    gToolbox.on("picker-started", this.onPickerStarted);
    gToolbox.on("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.addEventListener("click", this.toggleAll, false);
    gToolbox.target.on("navigate", this.onTabNavigated);
  },

  stopListeners: function() {
    AnimationsController.off(AnimationsController.PLAYERS_UPDATED_EVENT,
      this.createPlayerWidgets);

    this.pickerButtonEl.removeEventListener("click", this.togglePicker, false);
    gToolbox.off("picker-started", this.onPickerStarted);
    gToolbox.off("picker-stopped", this.onPickerStopped);

    this.toggleAllButtonEl.removeEventListener("click", this.toggleAll, false);
    gToolbox.target.off("navigate", this.onTabNavigated);
  },

  displayErrorMessage: function() {
    this.errorMessageEl.style.display = "block";
    this.playersEl.style.display = "none";
  },

  hideErrorMessage: function() {
    this.errorMessageEl.style.display = "none";
    this.playersEl.style.display = "block";
  },

  onPickerStarted: function() {
    this.pickerButtonEl.setAttribute("checked", "true");
  },

  onPickerStopped: function() {
    this.pickerButtonEl.removeAttribute("checked");
  },

  toggleAll: Task.async(function*() {
    let btnClass = this.toggleAllButtonEl.classList;

    // Toggling all animations is async and it may be some time before each of
    // the current players get their states updated, so toggle locally too, to
    // avoid the timelines from jumping back and forth.
    if (this.playerWidgets) {
      let currentWidgetStateChange = [];
      for (let widget of this.playerWidgets) {
        currentWidgetStateChange.push(btnClass.contains("paused")
          ? widget.play() : widget.pause());
      }
      yield promise.all(currentWidgetStateChange).catch(Cu.reportError);
    }

    btnClass.toggle("paused");
    yield AnimationsController.toggleAll();
  }),

  onTabNavigated: function() {
    this.toggleAllButtonEl.classList.remove("paused");
  },

  createPlayerWidgets: Task.async(function*() {
    let done = gInspector.updating("animationspanel");

    // Empty the whole panel first.
    this.hideErrorMessage();
    yield this.destroyPlayerWidgets();

    // If there are no players to show, show the error message instead and return.
    if (!AnimationsController.animationPlayers.length) {
      this.displayErrorMessage();
      this.emit(this.UI_UPDATED_EVENT);
      done();
      return;
    }

    // Otherwise, create player widgets.
    this.playerWidgets = [];
    let initPromises = [];

    for (let player of AnimationsController.animationPlayers) {
      let widget = new PlayerWidget(player, this.playersEl);
      initPromises.push(widget.initialize());
      this.playerWidgets.push(widget);
    }

    yield initPromises;
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

/**
 * An AnimationPlayer UI widget
 */
function PlayerWidget(player, containerEl) {
  EventEmitter.decorate(this);

  this.player = player;
  this.containerEl = containerEl;

  this.onStateChanged = this.onStateChanged.bind(this);
  this.onPlayPauseBtnClick = this.onPlayPauseBtnClick.bind(this);

  this.metaDataComponent = new PlayerMetaDataHeader();
}

PlayerWidget.prototype = {
  initialize: Task.async(function*() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    this.createMarkup();
    this.startListeners();
  }),

  destroy: Task.async(function*() {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;

    this.stopTimelineAnimation();
    this.stopListeners();
    this.metaDataComponent.destroy();

    this.el.remove();
    this.playPauseBtnEl = this.currentTimeEl = this.timeDisplayEl = null;
    this.containerEl = this.el = this.player = null;
  }),

  startListeners: function() {
    this.player.on(this.player.AUTO_REFRESH_EVENT, this.onStateChanged);
    this.playPauseBtnEl.addEventListener("click", this.onPlayPauseBtnClick);
  },

  stopListeners: function() {
    this.player.off(this.player.AUTO_REFRESH_EVENT, this.onStateChanged);
    this.playPauseBtnEl.removeEventListener("click", this.onPlayPauseBtnClick);
  },

  createMarkup: function() {
    let state = this.player.state;

    this.el = createNode({
      attributes: {
        "class": "player-widget " + state.playState
      }
    });

    this.metaDataComponent.createMarkup(this.el);
    this.metaDataComponent.render(state);

    // Timeline widget.
    let timelineEl = createNode({
      parent: this.el,
      attributes: {
        "class": "timeline"
      }
    });

    // Playback control buttons container.
    let playbackControlsEl = createNode({
      parent: timelineEl,
      attributes: {
        "class": "playback-controls"
      }
    });

    // Control buttons (when currentTime becomes settable, rewind and
    // fast-forward can be added here).
    this.playPauseBtnEl = createNode({
      parent: playbackControlsEl,
      nodeType: "button",
      attributes: {
        "class": "toggle devtools-button"
      }
    });

    // Sliders container.
    let slidersContainerEl = createNode({
      parent: timelineEl,
      attributes: {
        "class": "sliders-container",
      }
    });

    let max = state.duration;
    if (state.iterationCount) {
      // If there's a finite nb of iterations.
      max = state.iterationCount * state.duration;
    }

    // For now, keyframes aren't exposed by the actor. So the only range <input>
    // displayed in the container is the currentTime. When keyframes are
    // available, one input per keyframe can be added here.
    this.currentTimeEl = createNode({
      nodeType: "input",
      parent: slidersContainerEl,
      attributes: {
        "type": "range",
        "class": "current-time",
        "min": "0",
        "max": max,
        "step": "10",
        "value": "0",
        // The currentTime isn't settable yet, so disable the timeline slider
        "disabled": "true"
      }
    });

    // Time display
    this.timeDisplayEl = createNode({
      parent: timelineEl,
      attributes: {
        "class": "time-display"
      }
    });

    this.containerEl.appendChild(this.el);

    // Show the initial time.
    this.displayTime(state.currentTime);
  },

  /**
   * Executed when the playPause button is clicked.
   * Note that tests may want to call this callback directly rather than
   * simulating a click on the button since it returns the promise returned by
   * play and paused.
   * @return {Promise}
   */
  onPlayPauseBtnClick: function() {
    if (this.player.state.playState === "running") {
      return this.pause();
    } else {
      return this.play();
    }
  },

  /**
   * Whenever a player state update is received.
   */
  onStateChanged: function() {
    let state = this.player.state;
    this.updateWidgetState(state);
    this.metaDataComponent.render(state);

    switch (state.playState) {
      case "finished":
        this.stopTimelineAnimation();
        this.displayTime(this.player.state.duration);
        this.stopListeners();
        break;
      case "running":
        this.startTimelineAnimation();
        break;
      case "paused":
        this.stopTimelineAnimation();
        this.displayTime(this.player.state.currentTime);
        break;
    }
  },

  /**
   * Pause the animation player via this widget.
   * @return {Promise} Resolves when the player is paused, the button is
   * switched to the right state, and the timeline animation is stopped.
   */
  pause: function() {
    if (this.player.state.playState === "finished") {
      return;
    }

    // Switch to the right className on the element right away to avoid waiting
    // for the next state update to change the playPause icon.
    this.updateWidgetState({playState: "paused"});
    this.stopTimelineAnimation();
    return this.player.pause();
  },

  /**
   * Play the animation player via this widget.
   * @return {Promise} Resolves when the player is playing, the button is
   * switched to the right state, and the timeline animation is started.
   */
  play: function() {
    if (this.player.state.playState === "finished") {
      return;
    }

    // Switch to the right className on the element right away to avoid waiting
    // for the next state update to change the playPause icon.
    this.updateWidgetState({playState: "running"});
    this.startTimelineAnimation();
    return this.player.play();
  },

  updateWidgetState: function({playState}) {
    this.el.className = "player-widget " + playState;
  },

  /**
   * Make the timeline progress smoothly, even though the currentTime is only
   * updated at some intervals. This uses a local animation loop.
   */
  startTimelineAnimation: function() {
    this.stopTimelineAnimation();

    let state = this.player.state;

    let start = performance.now();
    let loop = () => {
      this.rafID = requestAnimationFrame(loop);
      let now = state.currentTime + performance.now() - start;
      this.displayTime(now);
    };

    loop();
  },

  /**
   * Display the time in the timeDisplayEl and in the currentTimeEl slider.
   */
  displayTime: function(time) {
    let state = this.player.state;

    // If the animation is delayed, don't start displaying the time until the
    // delay has passed.
    if (state.delay) {
      time = Math.max(0, time - state.delay);
    }

    // For finite animations, make sure the displayed time does not go beyond
    // the animation total duration (this may happen due to the local
    // requestAnimationFrame loop).
    if (state.iterationCount) {
      time = Math.min(time, state.iterationCount * state.duration);
    }

    // Set the time label value.
    this.timeDisplayEl.textContent = L10N.getFormatStr("player.timeLabel",
      L10N.numberWithDecimals(time / 1000, 2));

    // Set the timeline slider value.
    if (!state.iterationCount && time !== state.duration) {
      time = time % state.duration;
    }
    this.currentTimeEl.value = time;
  },

  /**
   * Stop the animation loop that makes the timeline progress.
   */
  stopTimelineAnimation: function() {
    if (this.rafID) {
      cancelAnimationFrame(this.rafID);
      this.rafID = null;
    }
  }
};

/**
 * UI component responsible for displaying and updating the player meta-data:
 * name, duration, iterations, delay.
 * The parent UI component for this should drive its updates by calling
 * render(state) whenever it wants the component to update.
 */
function PlayerMetaDataHeader() {
  // Store the various state pieces we need to only refresh the UI when things
  // change.
  this.state = {};
}

PlayerMetaDataHeader.prototype = {
  createMarkup: function(containerEl) {
    // The main title element.
    this.el = createNode({
      parent: containerEl,
      attributes: {
        "class": "animation-title"
      }
    });

    // Animation name (value hidden by default since transitions don't have names).
    this.nameLabel = createNode({
      parent: this.el,
      nodeType: "span"
    });

    this.nameValue = createNode({
      parent: this.el,
      nodeType: "strong",
      attributes: {
        "style": "display:none;"
      }
    });

    // Animation duration, delay and iteration container.
    let metaData = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "class": "meta-data"
      }
    });

    // Animation duration.
    this.durationLabel = createNode({
      parent: metaData,
      nodeType: "span"
    });
    this.durationLabel.textContent = L10N.getStr("player.animationDurationLabel");

    this.durationValue = createNode({
      parent: metaData,
      nodeType: "strong"
    });

    // Animation delay (hidden by default since there may not be a delay).
    this.delayLabel = createNode({
      parent: metaData,
      nodeType: "span",
      attributes: {
        "style": "display:none;"
      }
    });
    this.delayLabel.textContent = L10N.getStr("player.animationDelayLabel");

    this.delayValue = createNode({
      parent: metaData,
      nodeType: "strong"
    });

    // Animation iteration count (also hidden by default since we don't display
    // single iterations).
    this.iterationLabel = createNode({
      parent: metaData,
      nodeType: "span",
      attributes: {
        "style": "display:none;"
      }
    });
    this.iterationLabel.textContent = L10N.getStr("player.animationIterationCountLabel");

    this.iterationValue = createNode({
      parent: metaData,
      nodeType: "strong",
      attributes: {
        "style": "display:none;"
      }
    });
  },

  destroy: function() {
    this.state = null;
    this.el.remove();
    this.el = null;
    this.nameLabel = this.nameValue = null;
    this.durationLabel = this.durationValue = null;
    this.delayLabel = this.delayValue = null;
    this.iterationLabel = this.iterationValue = null;
  },

  render: function(state) {
    // Update the name if needed.
    if (state.name !== this.state.name) {
      if (state.name) {
        // Css animations have names.
        this.nameLabel.textContent = L10N.getStr("player.animationNameLabel");
        this.nameValue.style.display = "inline";
        this.nameValue.textContent = state.name;
      } else {
        // Css transitions don't.
        this.nameLabel.textContent = L10N.getStr("player.transitionNameLabel");
        this.nameValue.style.display = "none";
      }
    }

    // update the duration value if needed.
    if (state.duration !== this.state.duration) {
      this.durationValue.textContent = L10N.getFormatStr("player.timeLabel",
        L10N.numberWithDecimals(state.duration / 1000, 2));
    }

    // Update the delay if needed.
    if (state.delay !== this.state.delay) {
      if (state.delay) {
        this.delayLabel.style.display = "inline";
        this.delayValue.style.display = "inline";
        this.delayValue.textContent = L10N.getFormatStr("player.timeLabel",
          L10N.numberWithDecimals(state.delay / 1000, 2));
      } else {
        // Hide the delay elements if there is no delay defined.
        this.delayLabel.style.display = "none";
        this.delayValue.style.display = "none";
      }
    }

    // Update the iterationCount if needed.
    if (state.iterationCount !== this.state.iterationCount) {
      if (state.iterationCount !== 1) {
        this.iterationLabel.style.display = "inline";
        this.iterationValue.style.display = "inline";
        let count = state.iterationCount ||
                    L10N.getStr("player.infiniteIterationCount");
        this.iterationValue.innerHTML = count;
      } else {
        // Hide the iteration elements if iteration is 1.
        this.iterationLabel.style.display = "none";
        this.iterationValue.style.display = "none";
      }
    }

    this.state = state;
  }
};

/**
 * DOM node creation helper function.
 * @param {Object} Options to customize the node to be created.
 * @return {DOMNode} The newly created node.
 */
function createNode(options) {
  let type = options.nodeType || "div";
  let node = document.createElement(type);

  for (let name in options.attributes || {}) {
    let value = options.attributes[name];
    node.setAttribute(name, value);
  }

  if (options.parent) {
    options.parent.appendChild(node);
  }

  return node;
}
