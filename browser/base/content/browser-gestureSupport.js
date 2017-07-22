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
  _rotateMomentumThreshold: .75,

  /**
   * Add or remove mouse gesture event listeners
   *
   * @param aAddListener
   *        True to add/init listeners and false to remove/uninit
   */
  init: function GS_init(aAddListener) {
    const gestureEvents = ["SwipeGestureMayStart", "SwipeGestureStart",
      "SwipeGestureUpdate", "SwipeGestureEnd", "SwipeGesture",
      "MagnifyGestureStart", "MagnifyGestureUpdate", "MagnifyGesture",
      "RotateGestureStart", "RotateGestureUpdate", "RotateGesture",
      "TapGesture", "PressTapGesture"];

    let addRemove = aAddListener ? window.addEventListener :
      window.removeEventListener;

    for (let event of gestureEvents) {
      addRemove("Moz" + event, this, true);
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
    if (!Services.prefs.getBoolPref(
           "dom.debug.propagate_gesture_events_through_content")) {
      aEvent.stopPropagation();
    }

    // Create a preference object with some defaults
    let def = (aThreshold, aLatched) =>
      ({ threshold: aThreshold, latched: !!aLatched });

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
        let pinchPref = AppConstants.platform == "win"
                        ? def(25, 0)
                        : def(150, 1);
        this._setupGesture(aEvent, "pinch", pinchPref, "out", "in");
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
  _setupGesture: function GS__setupGesture(aEvent, aGesture, aPref, aInc, aDec) {
    // Try to load user-set values from preferences
    for (let [pref, def] of Object.entries(aPref))
      aPref[pref] = this._getPref(aGesture + "." + pref, def);

    // Keep track of the total deltas and latching behavior
    let offset = 0;
    let latchDir = aEvent.delta > 0 ? 1 : -1;
    let isLatched = false;

    // Create the update function here to capture closure state
    this._doUpdate = function GS__doUpdate(updateEvent) {
      // Update the offset with new event data
      offset += updateEvent.delta;

      // Check if the cumulative deltas exceed the threshold
      if (Math.abs(offset) > aPref["threshold"]) {
        // Trigger the action if we don't care about latching; otherwise, make
        // sure either we're not latched and going the same direction of the
        // initial motion; or we're latched and going the opposite way
        let sameDir = (latchDir ^ offset) >= 0;
        if (!aPref["latched"] || (isLatched ^ sameDir)) {
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
    return this._getCommand(aEvent, ["swipe", "left"])
              == "Browser:BackOrBackDuplicate" &&
           this._getCommand(aEvent, ["swipe", "right"])
              == "Browser:ForwardOrForwardDuplicate";
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
      if (gMultiProcessBrowser || window.content.pageYOffset < window.content.scrollMaxY) {
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
      aEvent.allowedDirections |= isLTR ? aEvent.DIRECTION_LEFT :
                                          aEvent.DIRECTION_RIGHT;
    }
    if (canGoForward) {
      aEvent.allowedDirections |= isLTR ? aEvent.DIRECTION_RIGHT :
                                          aEvent.DIRECTION_LEFT;
    }

    return true;
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
    gHistorySwipeAnimation.startAnimation(false);

    this._doUpdate = function GS__doUpdate(aEvent) {
      gHistorySwipeAnimation.updateAnimation(aEvent.delta);
    };

    this._doEnd = function GS__doEnd(aEvent) {
      gHistorySwipeAnimation.swipeEndEventReceived();

      this._doUpdate = function() {};
      this._doEnd = function() {};
    }
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
        if (num & 1 << aIndex)
          aPrev.push(aCurr);
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
      if (aEvent[key + "Key"])
        keyCombos.push(key);
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

      if (command)
        return command;
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
        cmdEvent.initCommandEvent("command", true, true, window, 0,
                                  aEvent.ctrlKey, aEvent.altKey,
                                  aEvent.shiftKey, aEvent.metaKey,
                                  aEvent, aEvent.mozInputSource);
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
  _coordinateSwipeEventWithAnimation:
  function GS__coordinateSwipeEventWithAnimation(aEvent, aDir) {
    if ((gHistorySwipeAnimation.isAnimationRunning()) &&
        (aDir == "RIGHT" || aDir == "LEFT")) {
      gHistorySwipeAnimation.processSwipeEvent(aEvent, aDir);
    } else {
      this.processSwipeEvent(aEvent, aDir);
    }
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
      if (type == "boolean")
        getFunc = "Bool";
      else if (type == "number")
        getFunc = "Int";
      return gPrefService["get" + getFunc + "Pref"](branch + aPref);
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
    if (!(window.content.document instanceof ImageDocument))
      return;

    let contentElement = window.content.document.body.firstElementChild;
    if (!contentElement)
      return;
    // If we're currently snapping, cancel that snap
    if (contentElement.classList.contains("completeRotation"))
      this._clearCompleteRotation();

    this.rotation = Math.round(this.rotation + aEvent.delta);
    contentElement.style.transform = "rotate(" + this.rotation + "deg)";
    this._lastRotateDelta = aEvent.delta;
  },

  /**
   * Perform a rotation end for ImageDocuments
   */
  rotateEnd() {
    if (!(window.content.document instanceof ImageDocument))
      return;

    let contentElement = window.content.document.body.firstElementChild;
    if (!contentElement)
      return;

    let transitionRotation = 0;

    // The reason that 360 is allowed here is because when rotating between
    // 315 and 360, setting rotate(0deg) will cause it to rotate the wrong
    // direction around--spinning wildly.
    if (this.rotation <= 45)
      transitionRotation = 0;
    else if (this.rotation > 45 && this.rotation <= 135)
      transitionRotation = 90;
    else if (this.rotation > 135 && this.rotation <= 225)
      transitionRotation = 180;
    else if (this.rotation > 225 && this.rotation <= 315)
      transitionRotation = 270;
    else
      transitionRotation = 360;

    // If we're going fast enough, and we didn't already snap ahead of rotation,
    // then snap ahead of rotation to simulate momentum
    if (this._lastRotateDelta > this._rotateMomentumThreshold &&
        this.rotation > transitionRotation)
      transitionRotation += 90;
    else if (this._lastRotateDelta < -1 * this._rotateMomentumThreshold &&
             this.rotation < transitionRotation)
      transitionRotation -= 90;

    // Only add the completeRotation class if it is is necessary
    if (transitionRotation != this.rotation) {
      contentElement.classList.add("completeRotation");
      contentElement.addEventListener("transitionend", this._clearCompleteRotation);
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
    if (this._currentRotation < 0)
      this._currentRotation += 360;
    return this._currentRotation;
  },

  /**
   * When the location/tab changes, need to reload the current rotation for the
   * image
   */
  restoreRotationState() {
    // Bug 1108553 - Cannot rotate images in stand-alone image documents with e10s
    if (gMultiProcessBrowser)
      return;

    if (!(window.content.document instanceof ImageDocument))
      return;

    let contentElement = window.content.document.body.firstElementChild;
    let transformValue = window.content.window.getComputedStyle(contentElement)
                                              .transform;

    if (transformValue == "none") {
      this.rotation = 0;
      return;
    }

    // transformValue is a rotation matrix--split it and do mathemagic to
    // obtain the real rotation value
    transformValue = transformValue.split("(")[1]
                                   .split(")")[0]
                                   .split(",");
    this.rotation = Math.round(Math.atan2(transformValue[1], transformValue[0]) *
                               (180 / Math.PI));
  },

  /**
   * Removes the transition rule by removing the completeRotation class
   */
  _clearCompleteRotation() {
    let contentElement = window.content.document &&
                         window.content.document instanceof ImageDocument &&
                         window.content.document.body &&
                         window.content.document.body.firstElementChild;
    if (!contentElement)
      return;
    contentElement.classList.remove("completeRotation");
    contentElement.removeEventListener("transitionend", this._clearCompleteRotation);
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
    if (!this._isSupported())
      return;

    this.active = false;
    this.isLTR = document.documentElement.matches(":-moz-locale-dir(ltr)");
    this._trackedSnapshots = [];
    this._startingIndex = -1;
    this._historyIndex = -1;
    this._boxWidth = -1;
    this._boxHeight = -1;
    this._maxSnapshots = this._getMaxSnapshots();
    this._lastSwipeDir = "";
    this._direction = "horizontal";

    // We only want to activate history swipe animations if we store snapshots.
    // If we don't store any, we handle horizontal swipes without animations.
    if (this._maxSnapshots > 0) {
      this.active = true;
      gBrowser.addEventListener("pagehide", this);
      gBrowser.addEventListener("pageshow", this);
      gBrowser.addEventListener("popstate", this);
      gBrowser.addEventListener("DOMModalDialogClosed", this);
      gBrowser.tabContainer.addEventListener("TabClose", this);
    }
  },

  /**
   * Uninitializes the support for history swipe animations.
   */
  uninit: function HSA_uninit() {
    gBrowser.removeEventListener("pagehide", this);
    gBrowser.removeEventListener("pageshow", this);
    gBrowser.removeEventListener("popstate", this);
    gBrowser.removeEventListener("DOMModalDialogClosed", this);
    gBrowser.tabContainer.removeEventListener("TabClose", this);

    this.active = false;
    this.isLTR = false;
  },

  /**
   * Starts the swipe animation and handles fast swiping (i.e. a swipe animation
   * is already in progress when a new one is initiated).
   *
   * @param aIsVerticalSwipe
   *        Whether we're dealing with a vertical swipe or not.
   */
  startAnimation: function HSA_startAnimation(aIsVerticalSwipe) {
    this._direction = aIsVerticalSwipe ? "vertical" : "horizontal";

    if (this.isAnimationRunning()) {
      // If this is a horizontal scroll, or if this is a vertical scroll that
      // was started while a horizontal scroll was still running, handle it as
      // as a fast swipe. In the case of the latter scenario, this allows us to
      // start the vertical animation without first loading the final page, or
      // taking another snapshot. If vertical scrolls are initiated repeatedly
      // without prior horizontal scroll we skip this and restart the animation
      // from 0.
      if (this._direction == "horizontal" || this._lastSwipeDir != "") {
        gBrowser.stop();
        this._lastSwipeDir = "RELOAD"; // just ensure that != ""
        this._canGoBack = this.canGoBack();
        this._canGoForward = this.canGoForward();
        this._handleFastSwiping();
      }
      this.updateAnimation(0);
    } else {
      // Get the session history from SessionStore.
      let updateSessionHistory = sessionHistory => {
        this._startingIndex = sessionHistory.index;
        this._historyIndex = this._startingIndex;
        this._canGoBack = this.canGoBack();
        this._canGoForward = this.canGoForward();
        if (this.active) {
          this._addBoxes();
          this._takeSnapshot();
          this._installPrevAndNextSnapshots();
          this._lastSwipeDir = "";
        }
        this.updateAnimation(0);
      }
      SessionStore.getSessionHistory(gBrowser.selectedTab, updateSessionHistory);
    }
  },

  /**
   * Stops the swipe animation.
   */
  stopAnimation: function HSA_stopAnimation() {
    gHistorySwipeAnimation._removeBoxes();
    this._historyIndex = this._getCurrentHistoryIndex();
  },

  /**
   * Updates the animation between two pages in history.
   *
   * @param aVal
   *        A floating point value that represents the progress of the
   *        swipe gesture.
   */
  updateAnimation: function HSA_updateAnimation(aVal) {
    if (!this.isAnimationRunning()) {
      return;
    }

    // We use the following value to decrease the bounce effect when scrolling
    // to the top or bottom of the page, or when swiping back/forward past the
    // browsing history. This value was determined experimentally.
    let dampValue = 4;
    if (this._direction == "vertical") {
      this._prevBox.collapsed = true;
      this._nextBox.collapsed = true;
      this._positionBox(this._curBox, -1 * aVal / dampValue);
    } else if ((aVal >= 0 && this.isLTR) ||
               (aVal <= 0 && !this.isLTR)) {
      let tempDampValue = 1;
      if (this._canGoBack) {
        this._prevBox.collapsed = false;
      } else {
        tempDampValue = dampValue;
        this._prevBox.collapsed = true;
      }

      // The current page is pushed to the right (LTR) or left (RTL),
      // the intention is to go back.
      // If there is a page to go back to, it should show in the background.
      this._positionBox(this._curBox, aVal / tempDampValue);

      // The forward page should be pushed offscreen all the way to the right.
      this._positionBox(this._nextBox, 1);
    } else if (this._canGoForward) {
      // The intention is to go forward. If there is a page to go forward to,
      // it should slide in from the right (LTR) or left (RTL).
      // Otherwise, the current page should slide to the left (LTR) or
      // right (RTL) and the backdrop should appear in the background.
      // For the backdrop to be visible in that case, the previous page needs
      // to be hidden (if it exists).
      this._nextBox.collapsed = false;
      let offset = this.isLTR ? 1 : -1;
      this._positionBox(this._curBox, 0);
      this._positionBox(this._nextBox, offset + aVal);
    } else {
      this._prevBox.collapsed = true;
      this._positionBox(this._curBox, aVal / dampValue);
    }
  },

  _getCurrentHistoryIndex() {
    return SessionStore.getSessionHistory(gBrowser.selectedTab).index;
  },

  /**
   * Event handler for events relevant to the history swipe animation.
   *
   * @param aEvent
   *        An event to process.
   */
  handleEvent: function HSA_handleEvent(aEvent) {
    let browser = gBrowser.selectedBrowser;
    switch (aEvent.type) {
      case "TabClose":
        let browserForTab = gBrowser.getBrowserForTab(aEvent.target);
        this._removeTrackedSnapshot(-1, browserForTab);
        break;
      case "DOMModalDialogClosed":
        this.stopAnimation();
        break;
      case "pageshow":
        if (aEvent.target == browser.contentDocument) {
          this.stopAnimation();
        }
        break;
      case "popstate":
        if (aEvent.target == browser.contentDocument.defaultView) {
          this.stopAnimation();
        }
        break;
      case "pagehide":
        if (aEvent.target == browser.contentDocument) {
          // Take and compress a snapshot of a page whenever it's about to be
          // navigated away from. We already have a snapshot of the page if an
          // animation is running, so we're left with compressing it.
          if (!this.isAnimationRunning()) {
            this._takeSnapshot();
          }
          this._compressSnapshotAtCurrentIndex();
        }
        break;
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
   * Process a swipe event based on the given direction.
   *
   * @param aEvent
   *        The swipe event to handle
   * @param aDir
   *        The direction for the swipe event
   */
  processSwipeEvent: function HSA_processSwipeEvent(aEvent, aDir) {
    if (aDir == "RIGHT")
      this._historyIndex += this.isLTR ? 1 : -1;
    else if (aDir == "LEFT")
      this._historyIndex += this.isLTR ? -1 : 1;
    else
      return;
    this._lastSwipeDir = aDir;
  },

  /**
   * Checks if there is a page in the browser history to go back to.
   *
   * @return true if there is a previous page in history, false otherwise.
   */
  canGoBack: function HSA_canGoBack() {
    if (this.isAnimationRunning())
      return this._doesIndexExistInHistory(this._historyIndex - 1);
    return gBrowser.webNavigation.canGoBack;
  },

  /**
   * Checks if there is a page in the browser history to go forward to.
   *
   * @return true if there is a next page in history, false otherwise.
   */
  canGoForward: function HSA_canGoForward() {
    if (this.isAnimationRunning())
      return this._doesIndexExistInHistory(this._historyIndex + 1);
    return gBrowser.webNavigation.canGoForward;
  },

  /**
   * Used to notify the history swipe animation that the OS sent a swipe end
   * event and that we should navigate to the page that the user swiped to, if
   * any. This will also result in the animation overlay to be torn down.
   */
  swipeEndEventReceived: function HSA_swipeEndEventReceived() {
    // Update the session history before continuing.
    let updateSessionHistory = sessionHistory => {
      if (this._lastSwipeDir != "" && this._historyIndex != this._startingIndex)
        this._navigateToHistoryIndex();
      else
        this.stopAnimation();
    }
    SessionStore.getSessionHistory(gBrowser.selectedTab, updateSessionHistory);
  },

  /**
   * Checks whether a particular index exists in the browser history or not.
   *
   * @param aIndex
   *        The index to check for availability for in the history.
   * @return true if the index exists in the browser history, false otherwise.
   */
  _doesIndexExistInHistory: function HSA__doesIndexExistInHistory(aIndex) {
    try {
      return SessionStore.getSessionHistory(gBrowser.selectedTab).entries[aIndex] != null;
    } catch (ex) {
      return false;
    }
  },

  /**
   * Navigates to the index in history that is currently being tracked by
   * |this|.
   */
  _navigateToHistoryIndex: function HSA__navigateToHistoryIndex() {
    if (this._doesIndexExistInHistory(this._historyIndex))
      gBrowser.webNavigation.gotoIndex(this._historyIndex);
    else
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

  /**
   * Handle fast swiping (i.e. a swipe animation is already in
   * progress when a new one is initiated). This will swap out the snapshots
   * used in the previous animation with the appropriate new ones.
   */
  _handleFastSwiping: function HSA__handleFastSwiping() {
    this._installCurrentPageSnapshot(null);
    this._installPrevAndNextSnapshots();
  },

  /**
   * Adds the boxes that contain the snapshots used during the swipe animation.
   */
  _addBoxes: function HSA__addBoxes() {
    let browserStack =
      document.getAnonymousElementByAttribute(gBrowser.getNotificationBox(),
                                              "class", "browserStack");
    this._container = this._createElement("historySwipeAnimationContainer",
                                          "stack");
    browserStack.appendChild(this._container);

    this._prevBox = this._createElement("historySwipeAnimationPreviousPage",
                                        "box");
    this._container.appendChild(this._prevBox);

    this._curBox = this._createElement("historySwipeAnimationCurrentPage",
                                       "box");
    this._container.appendChild(this._curBox);

    this._nextBox = this._createElement("historySwipeAnimationNextPage",
                                        "box");
    this._container.appendChild(this._nextBox);

    // Cache width and height.
    this._boxWidth = this._curBox.getBoundingClientRect().width;
    this._boxHeight = this._curBox.getBoundingClientRect().height;
  },

  /**
   * Removes the boxes.
   */
  _removeBoxes: function HSA__removeBoxes() {
    this._curBox = null;
    this._prevBox = null;
    this._nextBox = null;
    if (this._container)
      this._container.remove();
    this._container = null;
    this._boxWidth = -1;
    this._boxHeight = -1;
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
    let XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let element = document.createElementNS(XULNS, aTagName);
    element.id = aID;
    return element;
  },

  /**
   * Moves a given box to a given X coordinate position.
   *
   * @param aBox
   *        The box element to position.
   * @param aPosition
   *        The position (in X coordinates) to move the box element to.
   */
  _positionBox: function HSA__positionBox(aBox, aPosition) {
    let transform = "";

    if (this._direction == "vertical")
      transform = "translateY(" + this._boxHeight * aPosition + "px)";
    else
      transform = "translateX(" + this._boxWidth * aPosition + "px)";

    aBox.style.transform = transform;
  },

  /**
   * Verifies that we're ready to take snapshots based on the global pref and
   * the current index in history.
   *
   * @return true if we're ready to take snapshots, false otherwise.
   */
  _readyToTakeSnapshots: function HSA__readyToTakeSnapshots() {
    return (this._maxSnapshots >= 1 && this._getCurrentHistoryIndex() >= 0);
  },

  /**
   * Takes a snapshot of the page the browser is currently on.
   */
  _takeSnapshot: function HSA__takeSnapshot() {
    if (!this._readyToTakeSnapshots()) {
      return;
    }

    let canvas = null;

    let browser = gBrowser.selectedBrowser;
    let r = browser.getBoundingClientRect();
    canvas = document.createElementNS("http://www.w3.org/1999/xhtml",
                                      "canvas");
    canvas.mozOpaque = true;
    let scale = window.devicePixelRatio;
    canvas.width = r.width * scale;
    canvas.height = r.height * scale;
    let ctx = canvas.getContext("2d");
    let zoom = browser.markupDocumentViewer.fullZoom * scale;
    ctx.scale(zoom, zoom);
    ctx.drawWindow(browser.contentWindow,
                   0, 0, canvas.width / zoom, canvas.height / zoom, "white",
                   ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_VIEW |
                   ctx.DRAWWINDOW_ASYNC_DECODE_IMAGES |
                   ctx.DRAWWINDOW_USE_WIDGET_LAYERS);

    TelemetryStopwatch.start("FX_GESTURE_INSTALL_SNAPSHOT_OF_PAGE");
    try {
      this._installCurrentPageSnapshot(canvas);
      this._assignSnapshotToCurrentBrowser(canvas);
    } finally {
      TelemetryStopwatch.finish("FX_GESTURE_INSTALL_SNAPSHOT_OF_PAGE");
    }
  },

  /**
   * Retrieves the maximum number of snapshots that should be kept in memory.
   * This limit is a global limit and is valid across all open tabs.
   */
  _getMaxSnapshots: function HSA__getMaxSnapshots() {
    return gPrefService.getIntPref("browser.snapshots.limit");
  },

  /**
   * Adds a snapshot to the list and initiates the compression of said snapshot.
   * Once the compression is completed, it will replace the uncompressed
   * snapshot in the list.
   *
   * @param aCanvas
   *        The snapshot to add to the list and compress.
   */
  _assignSnapshotToCurrentBrowser:
  function HSA__assignSnapshotToCurrentBrowser(aCanvas) {
    let browser = gBrowser.selectedBrowser;
    let currIndex = this._getCurrentHistoryIndex();

    this._removeTrackedSnapshot(currIndex, browser);
    this._addSnapshotRefToArray(currIndex, browser);

    if (!("snapshots" in browser))
      browser.snapshots = [];
    let snapshots = browser.snapshots;
    // Temporarily store the canvas as the compressed snapshot.
    // This avoids a blank page if the user swipes quickly
    // between pages before the compression could complete.
    snapshots[currIndex] = {
      image: aCanvas,
      scale: window.devicePixelRatio
    };
  },

  /**
   * Compresses the HTMLCanvasElement that's stored at the current history
   * index in the snapshot array and stores the compressed image in its place.
   */
  _compressSnapshotAtCurrentIndex:
  function HSA__compressSnapshotAtCurrentIndex() {
    if (!this._readyToTakeSnapshots()) {
      // We didn't take a snapshot earlier because we weren't ready to, so
      // there's nothing to compress.
      return;
    }

    TelemetryStopwatch.start("FX_GESTURE_COMPRESS_SNAPSHOT_OF_PAGE");
    try {
      let browser = gBrowser.selectedBrowser;
      let snapshots = browser.snapshots;
      let currIndex = this._getCurrentHistoryIndex();

      // Kick off snapshot compression.
      let canvas = snapshots[currIndex].image;
      canvas.toBlob(function(aBlob) {
          if (snapshots[currIndex]) {
            snapshots[currIndex].image = aBlob;
          }
        }, "image/png"
      );
    } finally {
      TelemetryStopwatch.finish("FX_GESTURE_COMPRESS_SNAPSHOT_OF_PAGE");
    }
  },

  /**
   * Removes a snapshot identified by the browser and index in the array of
   * snapshots for that browser, if present. If no snapshot could be identified
   * the method simply returns without taking any action. If aIndex is negative,
   * all snapshots for a particular browser will be removed.
   *
   * @param aIndex
   *        The index in history of the new snapshot, or negative value if all
   *        snapshots for a browser should be removed.
   * @param aBrowser
   *        The browser the new snapshot was taken in.
   */
  _removeTrackedSnapshot: function HSA__removeTrackedSnapshot(aIndex, aBrowser) {
    let arr = this._trackedSnapshots;
    let requiresExactIndexMatch = aIndex >= 0;
    for (let i = 0; i < arr.length; i++) {
      if ((arr[i].browser == aBrowser) &&
          (aIndex < 0 || aIndex == arr[i].index)) {
        delete aBrowser.snapshots[arr[i].index];
        arr.splice(i, 1);
        if (requiresExactIndexMatch)
          return; // Found and removed the only element.
        i--; // Make sure to revisit the index that we just removed an
             // element at.
      }
    }
  },

  /**
   * Adds a new snapshot reference for a given index and browser to the array
   * of references to tracked snapshots.
   *
   * @param aIndex
   *        The index in history of the new snapshot.
   * @param aBrowser
   *        The browser the new snapshot was taken in.
   */
  _addSnapshotRefToArray:
  function HSA__addSnapshotRefToArray(aIndex, aBrowser) {
    let id = { index: aIndex,
               browser: aBrowser };
    let arr = this._trackedSnapshots;
    arr.unshift(id);

    while (arr.length > this._maxSnapshots) {
      let lastElem = arr[arr.length - 1];
      delete lastElem.browser.snapshots[lastElem.index].image;
      delete lastElem.browser.snapshots[lastElem.index];
      arr.splice(-1, 1);
    }
  },

  /**
   * Converts a compressed blob to an Image object. In some situations
   * (especially during fast swiping) aBlob may still be a canvas, not a
   * compressed blob. In this case, we simply return the canvas.
   *
   * @param aBlob
   *        The compressed blob to convert, or a canvas if a blob compression
   *        couldn't complete before this method was called.
   * @return A new Image object representing the converted blob.
   */
  _convertToImg: function HSA__convertToImg(aBlob) {
    if (!aBlob)
      return null;

    // Return aBlob if it's still a canvas and not a compressed blob yet.
    if (aBlob instanceof HTMLCanvasElement)
      return aBlob;

    let img = new Image();
    let url = "";
    try {
      url = URL.createObjectURL(aBlob);
      img.onload = function() {
        URL.revokeObjectURL(url);
      };
    } finally {
      img.src = url;
    }
    return img;
  },

  /**
   * Scales the background of a given box element (which uses a given snapshot
   * as background) based on a given scale factor.
   * @param aSnapshot
   *        The snapshot that is used as background of aBox.
   * @param aScale
   *        The scale factor to use.
   * @param aBox
   *        The box element that uses aSnapshot as background.
   */
  _scaleSnapshot: function HSA__scaleSnapshot(aSnapshot, aScale, aBox) {
    if (aSnapshot && aScale != 1 && aBox) {
      if (aSnapshot instanceof HTMLCanvasElement) {
        aBox.style.backgroundSize =
          aSnapshot.width / aScale + "px " + aSnapshot.height / aScale + "px";
      } else {
        // snapshot is instanceof HTMLImageElement
        aSnapshot.addEventListener("load", function() {
          aBox.style.backgroundSize =
            aSnapshot.width / aScale + "px " + aSnapshot.height / aScale + "px";
        });
      }
    }
  },

  /**
   * Sets the snapshot of the current page to the snapshot passed as parameter,
   * or to the one previously stored for the current index in history if the
   * parameter is null.
   *
   * @param aCanvas
   *        The snapshot to set the current page to. If this parameter is null,
   *        the previously stored snapshot for this index (if any) will be used.
   */
  _installCurrentPageSnapshot:
  function HSA__installCurrentPageSnapshot(aCanvas) {
    let currSnapshot = aCanvas;
    let scale = window.devicePixelRatio;
    if (!currSnapshot) {
      let snapshots = gBrowser.selectedBrowser.snapshots || {};
      let currIndex = this._historyIndex;
      if (currIndex in snapshots) {
        currSnapshot = this._convertToImg(snapshots[currIndex].image);
        scale = snapshots[currIndex].scale;
      }
    }
    this._scaleSnapshot(currSnapshot, scale, this._curBox ? this._curBox :
                                                            null);
    document.mozSetImageElement("historySwipeAnimationCurrentPageSnapshot",
                                currSnapshot);
  },

  /**
   * Sets the snapshots of the previous and next pages to the snapshots
   * previously stored for their respective indeces.
   */
  _installPrevAndNextSnapshots:
  function HSA__installPrevAndNextSnapshots() {
    let snapshots = gBrowser.selectedBrowser.snapshots || [];
    let currIndex = this._historyIndex;
    let prevIndex = currIndex - 1;
    let prevSnapshot = null;
    if (prevIndex in snapshots) {
      prevSnapshot = this._convertToImg(snapshots[prevIndex].image);
      this._scaleSnapshot(prevSnapshot, snapshots[prevIndex].scale,
                          this._prevBox);
    }
    document.mozSetImageElement("historySwipeAnimationPreviousPageSnapshot",
                                prevSnapshot);

    let nextIndex = currIndex + 1;
    let nextSnapshot = null;
    if (nextIndex in snapshots) {
      nextSnapshot = this._convertToImg(snapshots[nextIndex].image);
      this._scaleSnapshot(nextSnapshot, snapshots[nextIndex].scale,
                          this._nextBox);
    }
    document.mozSetImageElement("historySwipeAnimationNextPageSnapshot",
                                nextSnapshot);
  },
};
