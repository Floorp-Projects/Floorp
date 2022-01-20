/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

// Simple gestures support
//
// As per bug #412486, web content must not be allowed to receive any
// simple gesture events.  Multi-touch gesture APIs are in their
// infancy and we do NOT want to be forced into supporting an API that
// will probably have to change in the future.  (The current Mac OS X
// API is undocumented and was reverse-engineered.)  Until support is
// implemented in the event dispatcher to keep these events as
// chrome-only, we must listen for the simple gesture events during
// the capturing phase and call stopPropagation on every event.

var gGestureSupport = {
  _currentRotation: 0,
  _lastRotateDelta: 0,
  _rotateMomentumThreshold: 0.75,

  /**
   * Add or remove mouse gesture event listeners
   *
   * @param aAddListener
   *        True to add/init listeners and false to remove/uninit
   */
  init: function GS_init(aAddListener) {
    const gestureEvents = [
      "SwipeGestureMayStart",
      "SwipeGestureStart",
      "SwipeGestureUpdate",
      "SwipeGestureEnd",
      "SwipeGesture",
      "MagnifyGestureStart",
      "MagnifyGestureUpdate",
      "MagnifyGesture",
      "RotateGestureStart",
      "RotateGestureUpdate",
      "RotateGesture",
      "TapGesture",
      "PressTapGesture",
    ];

    for (let event of gestureEvents) {
      if (aAddListener) {
        gBrowser.tabbox.addEventListener("Moz" + event, this, true);
      } else {
        gBrowser.tabbox.removeEventListener("Moz" + event, this, true);
      }
    }
  },

  /**
   * Dispatch events based on the type of mouse gesture event. For now, make
   * sure to stop propagation of every gesture event so that web content cannot
   * receive gesture events.
   *
   * @param aEvent
   *        The gesture event to handle
   */
  handleEvent: function GS_handleEvent(aEvent) {
    if (
      !Services.prefs.getBoolPref(
        "dom.debug.propagate_gesture_events_through_content"
      )
    ) {
      aEvent.stopPropagation();
    }

    // Create a preference object with some defaults
    let def = (aThreshold, aLatched) => ({
      threshold: aThreshold,
      latched: !!aLatched,
    });

    switch (aEvent.type) {
      case "MozSwipeGestureMayStart":
        if (this._shouldDoSwipeGesture(aEvent)) {
          aEvent.preventDefault();
        }
        break;
      case "MozSwipeGestureStart":
        aEvent.preventDefault();
        this._setupSwipeGesture();
        break;
      case "MozSwipeGestureUpdate":
        aEvent.preventDefault();
        this._doUpdate(aEvent);
        break;
      case "MozSwipeGestureEnd":
        aEvent.preventDefault();
        this._doEnd(aEvent);
        break;
      case "MozSwipeGesture":
        aEvent.preventDefault();
        this.onSwipe(aEvent);
        break;
      case "MozMagnifyGestureStart":
        aEvent.preventDefault();
        this._setupGesture(aEvent, "pinch", def(25, 0), "out", "in");
        break;
      case "MozRotateGestureStart":
        aEvent.preventDefault();
        this._setupGesture(aEvent, "twist", def(25, 0), "right", "left");
        break;
      case "MozMagnifyGestureUpdate":
      case "MozRotateGestureUpdate":
        aEvent.preventDefault();
        this._doUpdate(aEvent);
        break;
      case "MozTapGesture":
        aEvent.preventDefault();
        this._doAction(aEvent, ["tap"]);
        break;
      case "MozRotateGesture":
        aEvent.preventDefault();
        this._doAction(aEvent, ["twist", "end"]);
        break;
      /* case "MozPressTapGesture":
        break; */
    }
  },

  /**
   * Called at the start of "pinch" and "twist" gestures to setup all of the
   * information needed to process the gesture
   *
   * @param aEvent
   *        The continual motion start event to handle
   * @param aGesture
   *        Name of the gesture to handle
   * @param aPref
   *        Preference object with the names of preferences and defaults
   * @param aInc
   *        Command to trigger for increasing motion (without gesture name)
   * @param aDec
   *        Command to trigger for decreasing motion (without gesture name)
   */
  _setupGesture: function GS__setupGesture(
    aEvent,
    aGesture,
    aPref,
    aInc,
    aDec
  ) {
    // Try to load user-set values from preferences
    for (let [pref, def] of Object.entries(aPref)) {
      aPref[pref] = this._getPref(aGesture + "." + pref, def);
    }

    // Keep track of the total deltas and latching behavior
    let offset = 0;
    let latchDir = aEvent.delta > 0 ? 1 : -1;
    let isLatched = false;

    // Create the update function here to capture closure state
    this._doUpdate = function GS__doUpdate(updateEvent) {
      // Update the offset with new event data
      offset += updateEvent.delta;

      // Check if the cumulative deltas exceed the threshold
      if (Math.abs(offset) > aPref.threshold) {
        // Trigger the action if we don't care about latching; otherwise, make
        // sure either we're not latched and going the same direction of the
        // initial motion; or we're latched and going the opposite way
        let sameDir = (latchDir ^ offset) >= 0;
        if (!aPref.latched || isLatched ^ sameDir) {
          this._doAction(updateEvent, [aGesture, offset > 0 ? aInc : aDec]);

          // We must be getting latched or leaving it, so just toggle
          isLatched = !isLatched;
        }

        // Reset motion counter to prepare for more of the same gesture
        offset = 0;
      }
    };

    // The start event also contains deltas, so handle an update right away
    this._doUpdate(aEvent);
  },

  /**
   * Checks whether a swipe gesture event can navigate the browser history or
   * not.
   *
   * @param aEvent
   *        The swipe gesture event.
   * @return true if the swipe event may navigate the history, false othwerwise.
   */
  _swipeNavigatesHistory: function GS__swipeNavigatesHistory(aEvent) {
    return (
      this._getCommand(aEvent, ["swipe", "left"]) ==
        "Browser:BackOrBackDuplicate" &&
      this._getCommand(aEvent, ["swipe", "right"]) ==
        "Browser:ForwardOrForwardDuplicate"
    );
  },

  /**
   * Checks whether we want to start a swipe for aEvent and sets
   * aEvent.allowedDirections to the right values.
   *
   * @param aEvent
   *        The swipe gesture "MayStart" event.
   * @return true if we're willing to start a swipe for this event, false
   *         otherwise.
   */
  _shouldDoSwipeGesture: function GS__shouldDoSwipeGesture(aEvent) {
    if (!this._swipeNavigatesHistory(aEvent)) {
      return false;
    }

    let isVerticalSwipe = false;
    if (aEvent.direction == aEvent.DIRECTION_UP) {
      if (gMultiProcessBrowser || window.content.pageYOffset > 0) {
        return false;
      }
      isVerticalSwipe = true;
    } else if (aEvent.direction == aEvent.DIRECTION_DOWN) {
      if (
        gMultiProcessBrowser ||
        window.content.pageYOffset < window.content.scrollMaxY
      ) {
        return false;
      }
      isVerticalSwipe = true;
    }
    if (isVerticalSwipe) {
      // Vertical overscroll has been temporarily disabled until bug 939480 is
      // fixed.
      return false;
    }

    let canGoBack = gHistorySwipeAnimation.canGoBack();
    let canGoForward = gHistorySwipeAnimation.canGoForward();
    let isLTR = gHistorySwipeAnimation.isLTR;

    if (canGoBack) {
      aEvent.allowedDirections |= isLTR
        ? aEvent.DIRECTION_LEFT
        : aEvent.DIRECTION_RIGHT;
    }
    if (canGoForward) {
      aEvent.allowedDirections |= isLTR
        ? aEvent.DIRECTION_RIGHT
        : aEvent.DIRECTION_LEFT;
    }

    return canGoBack || canGoForward;
  },

  /**
   * Sets up swipe gestures. This includes setting up swipe animations for the
   * gesture, if enabled.
   *
   * @param aEvent
   *        The swipe gesture start event.
   * @return true if swipe gestures could successfully be set up, false
   *         othwerwise.
   */
  _setupSwipeGesture: function GS__setupSwipeGesture() {
    gHistorySwipeAnimation.startAnimation();

    this._doUpdate = function GS__doUpdate(aEvent) {
      gHistorySwipeAnimation.updateAnimation(aEvent.delta);
    };

    this._doEnd = function GS__doEnd(aEvent) {
      gHistorySwipeAnimation.swipeEndEventReceived();

      this._doUpdate = function() {};
      this._doEnd = function() {};
    };
  },

  /**
   * Generator producing the powerset of the input array where the first result
   * is the complete set and the last result (before StopIteration) is empty.
   *
   * @param aArray
   *        Source array containing any number of elements
   * @yield Array that is a subset of the input array from full set to empty
   */
  _power: function* GS__power(aArray) {
    // Create a bitmask based on the length of the array
    let num = 1 << aArray.length;
    while (--num >= 0) {
      // Only select array elements where the current bit is set
      yield aArray.reduce(function(aPrev, aCurr, aIndex) {
        if (num & (1 << aIndex)) {
          aPrev.push(aCurr);
        }
        return aPrev;
      }, []);
    }
  },

  /**
   * Determine what action to do for the gesture based on which keys are
   * pressed and which commands are set, and execute the command.
   *
   * @param aEvent
   *        The original gesture event to convert into a fake click event
   * @param aGesture
   *        Array of gesture name parts (to be joined by periods)
   * @return Name of the executed command. Returns null if no command is
   *         found.
   */
  _doAction: function GS__doAction(aEvent, aGesture) {
    let command = this._getCommand(aEvent, aGesture);
    return command && this._doCommand(aEvent, command);
  },

  /**
   * Determine what action to do for the gesture based on which keys are
   * pressed and which commands are set
   *
   * @param aEvent
   *        The original gesture event to convert into a fake click event
   * @param aGesture
   *        Array of gesture name parts (to be joined by periods)
   */
  _getCommand: function GS__getCommand(aEvent, aGesture) {
    // Create an array of pressed keys in a fixed order so that a command for
    // "meta" is preferred over "ctrl" when both buttons are pressed (and a
    // command for both don't exist)
    let keyCombos = [];
    for (let key of ["shift", "alt", "ctrl", "meta"]) {
      if (aEvent[key + "Key"]) {
        keyCombos.push(key);
      }
    }

    // Try each combination of key presses in decreasing order for commands
    for (let subCombo of this._power(keyCombos)) {
      // Convert a gesture and pressed keys into the corresponding command
      // action where the preference has the gesture before "shift" before
      // "alt" before "ctrl" before "meta" all separated by periods
      let command;
      try {
        command = this._getPref(aGesture.concat(subCombo).join("."));
      } catch (e) {}

      if (command) {
        return command;
      }
    }
    return null;
  },

  /**
   * Execute the specified command.
   *
   * @param aEvent
   *        The original gesture event to convert into a fake click event
   * @param aCommand
   *        Name of the command found for the event's keys and gesture.
   */
  _doCommand: function GS__doCommand(aEvent, aCommand) {
    let node = document.getElementById(aCommand);
    if (node) {
      if (node.getAttribute("disabled") != "true") {
        let cmdEvent = document.createEvent("xulcommandevent");
        cmdEvent.initCommandEvent(
          "command",
          true,
          true,
          window,
          0,
          aEvent.ctrlKey,
          aEvent.altKey,
          aEvent.shiftKey,
          aEvent.metaKey,
          0,
          aEvent,
          aEvent.mozInputSource
        );
        node.dispatchEvent(cmdEvent);
      }
    } else {
      goDoCommand(aCommand);
    }
  },

  /**
   * Handle continual motion events.  This function will be set by
   * _setupGesture or _setupSwipe.
   *
   * @param aEvent
   *        The continual motion update event to handle
   */
  _doUpdate(aEvent) {},

  /**
   * Handle gesture end events.  This function will be set by _setupSwipe.
   *
   * @param aEvent
   *        The gesture end event to handle
   */
  _doEnd(aEvent) {},

  /**
   * Convert the swipe gesture into a browser action based on the direction.
   *
   * @param aEvent
   *        The swipe event to handle
   */
  onSwipe: function GS_onSwipe(aEvent) {
    // Figure out which one (and only one) direction was triggered
    for (let dir of ["UP", "RIGHT", "DOWN", "LEFT"]) {
      if (aEvent.direction == aEvent["DIRECTION_" + dir]) {
        this._coordinateSwipeEventWithAnimation(aEvent, dir);
        break;
      }
    }
  },

  /**
   * Process a swipe event based on the given direction.
   *
   * @param aEvent
   *        The swipe event to handle
   * @param aDir
   *        The direction for the swipe event
   */
  processSwipeEvent: function GS_processSwipeEvent(aEvent, aDir) {
    this._doAction(aEvent, ["swipe", aDir.toLowerCase()]);
  },

  /**
   * Coordinates the swipe event with the swipe animation, if any.
   * If an animation is currently running, the swipe event will be
   * processed once the animation stops. This will guarantee a fluid
   * motion of the animation.
   *
   * @param aEvent
   *        The swipe event to handle
   * @param aDir
   *        The direction for the swipe event
   */
  _coordinateSwipeEventWithAnimation: function GS__coordinateSwipeEventWithAnimation(
    aEvent,
    aDir
  ) {
    gHistorySwipeAnimation.stopAnimation();
    this.processSwipeEvent(aEvent, aDir);
  },

  /**
   * Get a gesture preference or use a default if it doesn't exist
   *
   * @param aPref
   *        Name of the preference to load under the gesture branch
   * @param aDef
   *        Default value if the preference doesn't exist
   */
  _getPref: function GS__getPref(aPref, aDef) {
    // Preferences branch under which all gestures preferences are stored
    const branch = "browser.gesture.";

    try {
      // Determine what type of data to load based on default value's type
      let type = typeof aDef;
      let getFunc = "Char";
      if (type == "boolean") {
        getFunc = "Bool";
      } else if (type == "number") {
        getFunc = "Int";
      }
      return Services.prefs["get" + getFunc + "Pref"](branch + aPref);
    } catch (e) {
      return aDef;
    }
  },

  /**
   * Perform rotation for ImageDocuments
   *
   * @param aEvent
   *        The MozRotateGestureUpdate event triggering this call
   */
  rotate(aEvent) {
    if (!(window.content.document instanceof ImageDocument)) {
      return;
    }

    let contentElement = window.content.document.body.firstElementChild;
    if (!contentElement) {
      return;
    }
    // If we're currently snapping, cancel that snap
    if (contentElement.classList.contains("completeRotation")) {
      this._clearCompleteRotation();
    }

    this.rotation = Math.round(this.rotation + aEvent.delta);
    contentElement.style.transform = "rotate(" + this.rotation + "deg)";
    this._lastRotateDelta = aEvent.delta;
  },

  /**
   * Perform a rotation end for ImageDocuments
   */
  rotateEnd() {
    if (!(window.content.document instanceof ImageDocument)) {
      return;
    }

    let contentElement = window.content.document.body.firstElementChild;
    if (!contentElement) {
      return;
    }

    let transitionRotation = 0;

    // The reason that 360 is allowed here is because when rotating between
    // 315 and 360, setting rotate(0deg) will cause it to rotate the wrong
    // direction around--spinning wildly.
    if (this.rotation <= 45) {
      transitionRotation = 0;
    } else if (this.rotation > 45 && this.rotation <= 135) {
      transitionRotation = 90;
    } else if (this.rotation > 135 && this.rotation <= 225) {
      transitionRotation = 180;
    } else if (this.rotation > 225 && this.rotation <= 315) {
      transitionRotation = 270;
    } else {
      transitionRotation = 360;
    }

    // If we're going fast enough, and we didn't already snap ahead of rotation,
    // then snap ahead of rotation to simulate momentum
    if (
      this._lastRotateDelta > this._rotateMomentumThreshold &&
      this.rotation > transitionRotation
    ) {
      transitionRotation += 90;
    } else if (
      this._lastRotateDelta < -1 * this._rotateMomentumThreshold &&
      this.rotation < transitionRotation
    ) {
      transitionRotation -= 90;
    }

    // Only add the completeRotation class if it is is necessary
    if (transitionRotation != this.rotation) {
      contentElement.classList.add("completeRotation");
      contentElement.addEventListener(
        "transitionend",
        this._clearCompleteRotation
      );
    }

    contentElement.style.transform = "rotate(" + transitionRotation + "deg)";
    this.rotation = transitionRotation;
  },

  /**
   * Gets the current rotation for the ImageDocument
   */
  get rotation() {
    return this._currentRotation;
  },

  /**
   * Sets the current rotation for the ImageDocument
   *
   * @param aVal
   *        The new value to take.  Can be any value, but it will be bounded to
   *        0 inclusive to 360 exclusive.
   */
  set rotation(aVal) {
    this._currentRotation = aVal % 360;
    if (this._currentRotation < 0) {
      this._currentRotation += 360;
    }
  },

  /**
   * When the location/tab changes, need to reload the current rotation for the
   * image
   */
  restoreRotationState() {
    // Bug 1108553 - Cannot rotate images in stand-alone image documents with e10s
    if (gMultiProcessBrowser) {
      return;
    }

    if (!(window.content.document instanceof ImageDocument)) {
      return;
    }

    let contentElement = window.content.document.body.firstElementChild;
    let transformValue = window.content.window.getComputedStyle(contentElement)
      .transform;

    if (transformValue == "none") {
      this.rotation = 0;
      return;
    }

    // transformValue is a rotation matrix--split it and do mathemagic to
    // obtain the real rotation value
    transformValue = transformValue
      .split("(")[1]
      .split(")")[0]
      .split(",");
    this.rotation = Math.round(
      Math.atan2(transformValue[1], transformValue[0]) * (180 / Math.PI)
    );
  },

  /**
   * Removes the transition rule by removing the completeRotation class
   */
  _clearCompleteRotation() {
    let contentElement =
      window.content.document &&
      window.content.document instanceof ImageDocument &&
      window.content.document.body &&
      window.content.document.body.firstElementChild;
    if (!contentElement) {
      return;
    }
    contentElement.classList.remove("completeRotation");
    contentElement.removeEventListener(
      "transitionend",
      this._clearCompleteRotation
    );
  },
};

// History Swipe Animation Support (bug 678392)
var gHistorySwipeAnimation = {
  active: false,
  isLTR: false,

  /**
   * Initializes the support for history swipe animations, if it is supported
   * by the platform/configuration.
   */
  init: function HSA_init() {
    if (!this._isSupported()) {
      return;
    }

    this.isLTR = document.documentElement.matches(":-moz-locale-dir(ltr)");
    this._isStoppingAnimation = false;
    if (
      !Services.prefs.getBoolPref(
        "browser.history_swipe_animation.disabled",
        false
      )
    ) {
      this.active = true;
    }
  },

  /**
   * Uninitializes the support for history swipe animations.
   */
  uninit: function HSA_uninit() {
    this.active = false;
    this.isLTR = false;
    this._removeBoxes();
  },

  /**
   * Starts the swipe animation.
   *
   * @param aIsVerticalSwipe
   *        Whether we're dealing with a vertical swipe or not.
   */
  startAnimation: function HSA_startAnimation() {
    if (this.isAnimationRunning()) {
      return;
    }

    this._isStoppingAnimation = false;
    this._canGoBack = this.canGoBack();
    this._canGoForward = this.canGoForward();
    if (this.active) {
      this._addBoxes();
    }
    this.updateAnimation(0);
  },

  /**
   * Stops the swipe animation.
   */
  stopAnimation: function HSA_stopAnimation() {
    if (!this.isAnimationRunning()) {
      return;
    }
    this._isStoppingAnimation = true;
    let box = this._prevBox.style.opacity > 0 ? this._prevBox : this._nextBox;
    if (box.style.opacity > 0) {
      box.style.transition = "opacity 0.2s cubic-bezier(.07,.95,0,1)";
      box.addEventListener("transitionend", this._completeFadeOut);
      box.style.opacity = 0;
    } else {
      this._removeBoxes();
    }
  },

  /**
   * Updates the animation between two pages in history.
   *
   * @param aVal
   *        A floating point value that represents the progress of the
   *        swipe gesture.
   */
  updateAnimation: function HSA_updateAnimation(aVal) {
    if (!this.isAnimationRunning() || this._isStoppingAnimation) {
      return;
    }

    // We use the following value to set the opacity of the swipe arrows. It was
    // determined experimentally that absolute values of 0.25 (or greater)
    // trigger history navigation, hence the multiplier 4 to set the arrows to
    // full opacity at 0.25 or greater.
    let opacity = Math.abs(aVal) * 4;
    if ((aVal >= 0 && this.isLTR) || (aVal <= 0 && !this.isLTR)) {
      // The intention is to go back.
      if (this._canGoBack) {
        this._prevBox.collapsed = false;
        this._nextBox.collapsed = true;
        this._prevBox.style.opacity = opacity > 1 ? 1 : opacity;
      }
    } else if (this._canGoForward) {
      // The intention is to go forward.
      this._nextBox.collapsed = false;
      this._prevBox.collapsed = true;
      this._nextBox.style.opacity = opacity > 1 ? 1 : opacity;
    }
  },

  /**
   * Checks whether the history swipe animation is currently running or not.
   *
   * @return true if the animation is currently running, false otherwise.
   */
  isAnimationRunning: function HSA_isAnimationRunning() {
    return !!this._container;
  },

  /**
   * Checks if there is a page in the browser history to go back to.
   *
   * @return true if there is a previous page in history, false otherwise.
   */
  canGoBack: function HSA_canGoBack() {
    return gBrowser.webNavigation.canGoBack;
  },

  /**
   * Checks if there is a page in the browser history to go forward to.
   *
   * @return true if there is a next page in history, false otherwise.
   */
  canGoForward: function HSA_canGoForward() {
    return gBrowser.webNavigation.canGoForward;
  },

  /**
   * Used to notify the history swipe animation that the OS sent a swipe end
   * event and that we should navigate to the page that the user swiped to, if
   * any. This will also result in the animation overlay to be torn down.
   */
  swipeEndEventReceived: function HSA_swipeEndEventReceived() {
    this.stopAnimation();
  },

  /**
   * Checks to see if history swipe animations are supported by this
   * platform/configuration.
   *
   * return true if supported, false otherwise.
   */
  _isSupported: function HSA__isSupported() {
    return window.matchMedia("(-moz-swipe-animation-enabled)").matches;
  },

  _completeFadeOut: function HSA__completeFadeOut(aEvent) {
    gHistorySwipeAnimation._removeBoxes();
  },

  /**
   * Adds the boxes that contain the arrows used during the swipe animation.
   */
  _addBoxes: function HSA__addBoxes() {
    let browserStack = gBrowser.getPanel().querySelector(".browserStack");
    this._container = this._createElement(
      "historySwipeAnimationContainer",
      "stack"
    );
    browserStack.appendChild(this._container);

    this._prevBox = this._createElement(
      "historySwipeAnimationPreviousArrow",
      "box"
    );
    this._prevBox.collapsed = true;
    this._prevBox.style.opacity = 0;
    this._container.appendChild(this._prevBox);

    this._nextBox = this._createElement(
      "historySwipeAnimationNextArrow",
      "box"
    );
    this._nextBox.collapsed = true;
    this._nextBox.style.opacity = 0;
    this._container.appendChild(this._nextBox);
  },

  /**
   * Removes the boxes.
   */
  _removeBoxes: function HSA__removeBoxes() {
    this._prevBox = null;
    this._nextBox = null;
    if (this._container) {
      this._container.remove();
    }
    this._container = null;
  },

  /**
   * Creates an element with a given identifier and tag name.
   *
   * @param aID
   *        An identifier to create the element with.
   * @param aTagName
   *        The name of the tag to create the element for.
   * @return the newly created element.
   */
  _createElement: function HSA__createElement(aID, aTagName) {
    let element = document.createXULElement(aTagName);
    element.id = aID;
    return element;
  },
};
