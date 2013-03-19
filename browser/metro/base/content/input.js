// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; js2-strict-trailing-comma-warning: nil -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Geometry.jsm");

/*
 * Drag scrolling related constants
 */

// maximum drag distance in inches while axis locking can still be reverted
const kAxisLockRevertThreshold = 0.8;

// Same as NS_EVENT_STATE_ACTIVE from nsIEventStateManager.h
const kStateActive = 0x00000001;

// After a drag begins, kinetic panning is stopped if the drag doesn't become
// a pan in 300 milliseconds.
const kStopKineticPanOnDragTimeout = 300;

// Min/max velocity of kinetic panning. This is in pixels/millisecond.
const kMinVelocity = 0.4;
const kMaxVelocity = 6;

/*
 * prefs
 */

// A debug pref that when set makes us treat all precise pointer input
// as imprecise touch input. For debugging purposes only. Note there are
// subtle event sequencing differences in this feature when running on
// the desktop using the win32 widget backend and the winrt widget backend
// in metro. Fixing something in this mode does not insure the bug is
// in metro.
const kDebugMouseInputPref = "metro.debug.treatmouseastouch";
// Display rects around selection ranges. Useful in debugging
// selection problems.
const kDebugSelectionDisplayPref = "metro.debug.selection.displayRanges";
// Dump range rect data to the console. Very useful, but also slows
// things down a lot.
const kDebugSelectionDumpPref = "metro.debug.selection.dumpRanges";
// Dump message manager event traffic for selection.
const kDebugSelectionDumpEvents = "metro.debug.selection.dumpEvents";

/**
 * TouchModule
 *
 * Handles all touch-related input such as dragging and tapping.
 *
 * The Fennec chrome DOM tree has elements that are augmented dynamically with
 * custom JS properties that tell the TouchModule they have custom support for
 * either dragging or clicking.  These JS properties are JS objects that expose
 * an interface supporting dragging or clicking (though currently we only look
 * to drag scrollable elements).
 *
 * A custom dragger is a JS property that lives on a scrollable DOM element,
 * accessible as myElement.customDragger.  The customDragger must support the
 * following interface:  (The `scroller' argument is given for convenience, and
 * is the object reference to the element's scrollbox object).
 *
 *   dragStart(cX, cY, target, scroller)
 *     Signals the beginning of a drag.  Coordinates are passed as
 *     client coordinates. target is copied from the event.
 *
 *   dragStop(dx, dy, scroller)
 *     Signals the end of a drag.  The dx, dy parameters may be non-zero to
 *     indicate one last drag movement.
 *
 *   dragMove(dx, dy, scroller, isKinetic)
 *     Signals an input attempt to drag by dx, dy.
 *
 * There is a default dragger in case a scrollable element is dragged --- see
 * the defaultDragger prototype property.
 */

var TouchModule = {
  _debugEvents: false,
  _isCancelled: false,
  _isCancellable: false,

  init: function init() {
    this._dragData = new DragData();

    this._dragger = null;

    this._targetScrollbox = null;
    this._targetScrollInterface = null;

    this._kinetic = new KineticController(this._dragBy.bind(this),
                                          this._kineticStop.bind(this));

    // capture phase events
    window.addEventListener("CancelTouchSequence", this, true);
    window.addEventListener("dblclick", this, true);

    // bubble phase
    window.addEventListener("contextmenu", this, false);
    window.addEventListener("touchstart", this, false);
    window.addEventListener("touchmove", this, false);
    window.addEventListener("touchend", this, false);

    try {
      this._treatMouseAsTouch = Services.prefs.getBoolPref(kDebugMouseInputPref);
    } catch (e) {}
  },

  /*
   * Mouse input source tracking
   */

  _treatMouseAsTouch: false,

  /*
   * Events
   */

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "contextmenu":
        this._onContextMenu(aEvent);
        break;

      case "CancelTouchSequence":
        this.cancelPending();
        break;

      default: {
        if (this._debugEvents) {
          if (aEvent.type != "touchmove")
            Util.dumpLn("TouchModule:", aEvent.type, aEvent.target);
        }

        switch (aEvent.type) {
          case "touchstart":
            this._onTouchStart(aEvent);
            break;
          case "touchmove":
            this._onTouchMove(aEvent);
            break;
          case "touchend":
            this._onTouchEnd(aEvent);
            break;
          case "dblclick":
            // XXX This will get picked up somewhere below us for "double tap to zoom"
            // once we get omtc and the apzc. Currently though dblclick is delivered to
            // content and triggers selection of text, so fire up the SelectionHelperUI
            // once selection is present.
            setTimeout(function () {
              let contextInfo = { name: "",
                                  json: { xPos: aEvent.clientX, yPos: aEvent.clientY },
                                  target: Browser.selectedTab.browser };
              SelectionHelperUI.attachEditSession(contextInfo);
            }, 50);
            break;
        }
      }
    }
  },

  sample: function sample(aTimeStamp) {
    this._waitingForPaint = false;
  },

  /**
   * This gets invoked by the input handler if another module grabs.  We should
   * reset our state or something here.  This is probably doing the wrong thing
   * in its current form.
   */
  cancelPending: function cancelPending() {
    this._doDragStop();

    // Kinetic panning may have already been active or drag stop above may have
    // made kinetic panning active.
    this._kinetic.end();

    this._targetScrollbox = null;
    this._targetScrollInterface = null;
  },

  _onContextMenu: function _onContextMenu(aEvent) {
    // Special case when running on the desktop, fire off
    // a edge ui event when we get the contextmenu event.
    if (this._treatMouseAsTouch) {
      let event = document.createEvent("Events");
      event.initEvent("MozEdgeUIGesture", true, false);
      window.dispatchEvent(event);
      return;
    }

    // bug 598965 - chrome UI should stop to be pannable once the
    // context menu has appeared.
    if (ContextMenuUI.popupState) {
      this.cancelPending();
    }
  },

  /** Begin possible pan and send tap down event. */
  _onTouchStart: function _onTouchStart(aEvent) {
    if (aEvent.touches.length > 1)
      return;

    this._isCancelled = false;
    this._isCancellable = true;

    if (aEvent.defaultPrevented) {
      this._isCancelled = true;
      return;
    }

    let dragData = this._dragData;
    if (dragData.dragging) {
      // Somehow a mouse up was missed.
      this._doDragStop();
    }
    dragData.reset();
    this.dX = 0;
    this.dY = 0;

    // walk up the DOM tree in search of nearest scrollable ancestor.  nulls are
    // returned if none found.
    let [targetScrollbox, targetScrollInterface, dragger]
      = ScrollUtils.getScrollboxFromElement(aEvent.originalTarget);

    // stop kinetic panning if targetScrollbox has changed
    if (this._kinetic.isActive() && this._dragger != dragger)
      this._kinetic.end();

    this._targetScrollbox = targetScrollInterface ? targetScrollInterface.element : targetScrollbox;
    this._targetScrollInterface = targetScrollInterface;

    if (!this._targetScrollbox) {
      return;
    }

    // XXX shouldn't dragger always be valid here?
    if (dragger) {
      let draggable = dragger.isDraggable(targetScrollbox, targetScrollInterface);
      dragData.locked = !draggable.x || !draggable.y;
      if (draggable.x || draggable.y) {
        this._dragger = dragger;
        if (dragger.freeDrag)
          dragData.alwaysFreeDrag = dragger.freeDrag();
        this._doDragStart(aEvent, draggable);
      }
    }
  },

  /** Send tap up event and any necessary full taps. */
  _onTouchEnd: function _onTouchEnd(aEvent) {
    if (aEvent.touches.length > 0 || this._isCancelled || !this._targetScrollbox)
      return;

    // onMouseMove will not record the delta change if we are waiting for a
    // paint. Since this is the last input for this drag, we override the flag.
    this._waitingForPaint = false;
    this._onTouchMove(aEvent);

    let dragData = this._dragData;
    this._doDragStop();
  },

  /**
   * If we're in a drag, do what we have to do to drag on.
   */
  _onTouchMove: function _onTouchMove(aEvent) {
    if (aEvent.touches.length > 1)
      return;

    if (this._isCancellable) {
      // only the first touchmove is cancellable.
      this._isCancellable = false;
      if (aEvent.defaultPrevented)
        this._isCancelled = true;
    }

    if (this._isCancelled)
      return;

    let touch = aEvent.changedTouches[0];
    if (!this._targetScrollbox) {
      return;
    }

    let dragData = this._dragData;

    if (dragData.dragging) {
      let oldIsPan = dragData.isPan();
      dragData.setDragPosition(touch.screenX, touch.screenY);
      dragData.setMousePosition(touch);

      // Kinetic panning is sensitive to time. It is more stable if it receives
      // the mousemove events as they come. For dragging though, we only want
      // to call _dragBy if we aren't waiting for a paint (so we don't spam the
      // main browser loop with a bunch of redundant paints).
      //
      // Here, we feed kinetic panning drag differences for mouse events as
      // come; for dragging, we build up a drag buffer in this.dX/this.dY and
      // release it when we are ready to paint.
      //
      let [sX, sY] = dragData.panPosition();
      this.dX += dragData.prevPanX - sX;
      this.dY += dragData.prevPanY - sY;

      if (dragData.isPan()) {
        // Only pan when mouse event isn't part of a click. Prevent jittering on tap.
        this._kinetic.addData(sX - dragData.prevPanX, sY - dragData.prevPanY);

        // dragBy will reset dX and dY values to 0
        this._dragBy(this.dX, this.dY);

        // Let everyone know when mousemove begins a pan
        if (!oldIsPan && dragData.isPan()) {
          //this._longClickTimeout.clear();

          let event = document.createEvent("Events");
          event.initEvent("PanBegin", true, false);
          this._targetScrollbox.dispatchEvent(event);

          Browser.selectedBrowser.messageManager.sendAsyncMessage("Browser:PanBegin", {});
        }
      }
    }
  },

  /**
   * Inform our dragger of a dragStart.
   */
  _doDragStart: function _doDragStart(aEvent, aDraggable) {
    let touch = aEvent.changedTouches[0];
    let dragData = this._dragData;
    dragData.setDragStart(touch.screenX, touch.screenY, aDraggable);
    this._kinetic.addData(0, 0);
    this._dragStartTime = Date.now();
    if (!this._kinetic.isActive()) {
      this._dragger.dragStart(touch.clientX, touch.clientY, touch.target, this._targetScrollInterface);
    }
  },

  /** Finish a drag. */
  _doDragStop: function _doDragStop() {
    let dragData = this._dragData;
    if (!dragData.dragging)
      return;

    dragData.endDrag();

    // Note: it is possible for kinetic scrolling to be active from a
    // mousedown/mouseup event previous to this one. In this case, we
    // want the kinetic panner to tell our drag interface to stop.

    if (dragData.isPan()) {
      if (Date.now() - this._dragStartTime > kStopKineticPanOnDragTimeout)
        this._kinetic._velocity.set(0, 0);
      // Start kinetic pan.
      this._kinetic.start();
    } else {
      this._kinetic.end();
      if (this._dragger)
        this._dragger.dragStop(0, 0, this._targetScrollInterface);
      this._dragger = null;
    }
  },

  /**
   * Used by _onTouchMove() above and by KineticController's timer to do the
   * actual dragMove signalling to the dragger.  We'd put this in _onTouchMove()
   * but then KineticController would be adding to its own data as it signals
   * the dragger of dragMove()s.
   */
  _dragBy: function _dragBy(dX, dY, aIsKinetic) {
    let dragged = true;
    let dragData = this._dragData;
    if (!this._waitingForPaint || aIsKinetic) {
      let dragData = this._dragData;
      dragged = this._dragger.dragMove(dX, dY, this._targetScrollInterface, aIsKinetic,
                                       dragData._mouseX, dragData._mouseY);
      if (dragged && !this._waitingForPaint) {
        this._waitingForPaint = true;
        mozRequestAnimationFrame(this);
      }
      this.dX = 0;
      this.dY = 0;
    }
    if (!dragData.isPan())
      this._kinetic.pause();

    return dragged;
  },

  /** Callback for kinetic scroller. */
  _kineticStop: function _kineticStop() {
    // Kinetic panning could finish while user is panning, so don't finish
    // the pan just yet.
    let dragData = this._dragData;
    if (!dragData.dragging) {
      if (this._dragger)
        this._dragger.dragStop(0, 0, this._targetScrollInterface);
      this._dragger = null;

      let event = document.createEvent("Events");
      event.initEvent("PanFinished", true, false);
      this._targetScrollbox.dispatchEvent(event);
    }
  },

  toString: function toString() {
    return '[TouchModule] {'
      + '\n\tdragData=' + this._dragData + ', '
      + 'dragger=' + this._dragger + ', '
      + '\n\ttargetScroller=' + this._targetScrollInterface + '}';
  },
};

var ScrollUtils = {
  // threshold in pixels for sensing a tap as opposed to a pan
  get tapRadius() {
    let dpi = Util.displayDPI;
    delete this.tapRadius;
    return this.tapRadius = Services.prefs.getIntPref("ui.dragThresholdX") / 240 * dpi;
  },

  /**
   * Walk up (parentward) the DOM tree from elem in search of a scrollable element.
   * Return the element and its scroll interface if one is found, two nulls otherwise.
   *
   * This function will cache the pointer to the scroll interface on the element itself,
   * so it is safe to call it many times without incurring the same XPConnect overhead
   * as in the initial call.
   */
  getScrollboxFromElement: function getScrollboxFromElement(elem) {
    let scrollbox = null;
    let qinterface = null;
    // if element is content, get the browser scroll interface
    if (elem.ownerDocument == Browser.selectedBrowser.contentDocument) {
      elem = Browser.selectedBrowser;
    }
    for (; elem; elem = elem.parentNode) {
      try {
        if (elem.anonScrollBox) {
          scrollbox = elem.anonScrollBox;
          qinterface = scrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
        } else if (elem.scrollBoxObject) {
          scrollbox = elem;
          qinterface = elem.scrollBoxObject;
          break;
        } else if (elem.customDragger) {
          scrollbox = elem;
          break;
        } else if (elem.boxObject) {
          let qi = (elem._cachedSBO) ? elem._cachedSBO
                                     : elem.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
          if (qi) {
            scrollbox = elem;
            scrollbox._cachedSBO = qinterface = qi;
            break;
          }
        }
      } catch (e) { /* we aren't here to deal with your exceptions, we'll just keep
                       traversing until we find something more well-behaved, as we
                       prefer default behaviour to whiny scrollers. */ }
    }
    return [scrollbox, qinterface, (scrollbox ? (scrollbox.customDragger || this._defaultDragger) : null)];
  },

  /** Determine if the distance moved can be considered a pan */
  isPan: function isPan(aPoint, aPoint2) {
    return (Math.abs(aPoint.x - aPoint2.x) > this.tapRadius ||
            Math.abs(aPoint.y - aPoint2.y) > this.tapRadius);
  },

  /**
   * The default dragger object used by TouchModule when dragging a scrollable
   * element that provides no customDragger.  Simply performs the expected
   * regular scrollBy calls on the scroller.
   */
  _defaultDragger: {
    isDraggable: function isDraggable(target, scroller) {
      let sX = {}, sY = {},
          pX = {}, pY = {};
      scroller.getPosition(pX, pY);
      scroller.getScrolledSize(sX, sY);
      let rect = target.getBoundingClientRect();
      return { x: (sX.value > rect.width  || pX.value != 0),
               y: (sY.value > rect.height || pY.value != 0) };
    },

    dragStart: function dragStart(cx, cy, target, scroller) {
      scroller.element.addEventListener("PanBegin", this._showScrollbars, false);
    },

    dragStop: function dragStop(dx, dy, scroller) {
      scroller.element.removeEventListener("PanBegin", this._showScrollbars, false);
      return this.dragMove(dx, dy, scroller);
    },

    dragMove: function dragMove(dx, dy, scroller) {
      if (scroller.getPosition) {
        try {
          let oldX = {}, oldY = {};
          scroller.getPosition(oldX, oldY);

          scroller.scrollBy(dx, dy);

          let newX = {}, newY = {};
          scroller.getPosition(newX, newY);

          return (newX.value != oldX.value) || (newY.value != oldY.value);

        } catch (e) { /* we have no time for whiny scrollers! */ }
      }

      return false;
    },

    _showScrollbars: function _showScrollbars(aEvent) {
      let scrollbox = aEvent.target;
      scrollbox.setAttribute("panning", "true");

      let hideScrollbars = function() {
        scrollbox.removeEventListener("PanFinished", hideScrollbars, false);
        scrollbox.removeEventListener("CancelTouchSequence", hideScrollbars, false);
        scrollbox.removeAttribute("panning");
      }

      // Wait for panning to be completely finished before removing scrollbars
      scrollbox.addEventListener("PanFinished", hideScrollbars, false);
      scrollbox.addEventListener("CancelTouchSequence", hideScrollbars, false);
    }
  }
};

/**
 * DragData handles processing drags on the screen, handling both
 * locking of movement on one axis, and click detection.
 */
function DragData() {
  this._domUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
  this._lockRevertThreshold = Util.displayDPI * kAxisLockRevertThreshold;
  this.reset();
};

DragData.prototype = {
  reset: function reset() {
    this.dragging = false;
    this.sX = null;
    this.sY = null;
    this.locked = false;
    this.stayLocked = false;
    this.alwaysFreeDrag = false;
    this.lockedX = null;
    this.lockedY = null;
    this._originX = null;
    this._originY = null;
    this.prevPanX = null;
    this.prevPanY = null;
    this._isPan = false;
  },

  /** Depending on drag data, locks sX,sY to X-axis or Y-axis of start position. */
  _lockAxis: function _lockAxis(sX, sY) {
    if (this.locked) {
      if (this.lockedX !== null)
        sX = this.lockedX;
      else if (this.lockedY !== null)
        sY = this.lockedY;
      return [sX, sY];
    }
    else {
      return [this._originX, this._originY];
    }
  },

  setMousePosition: function setMousePosition(aEvent) {
    this._mouseX = aEvent.clientX;
    this._mouseY = aEvent.clientY;
  },

  setDragPosition: function setDragPosition(sX, sY) {
    // Check if drag is now a pan.
    if (!this._isPan) {
      this._isPan = ScrollUtils.isPan(new Point(this._originX, this._originY), new Point(sX, sY));
      if (this._isPan) {
        this._resetActive();
      }
    }

    // If now a pan, mark previous position where panning was.
    if (this._isPan) {
      let absX = Math.abs(this._originX - sX);
      let absY = Math.abs(this._originY - sY);

      if (absX > this._lockRevertThreshold || absY > this._lockRevertThreshold)
        this.stayLocked = true;

      // After the first lock, see if locking decision should be reverted.
      if (!this.stayLocked) {
        if (this.lockedX && absX > 3 * absY)
          this.lockedX = null;
        else if (this.lockedY && absY > 3 * absX)
          this.lockedY = null;
      }

      if (!this.locked) {
        // look at difference from origin coord to lock movement, but only
        // do it if initial movement is sufficient to detect intent

        // divide possibilty space into eight parts.  Diagonals will allow
        // free movement, while moving towards a cardinal will lock that
        // axis.  We pick a direction if you move more than twice as far
        // on one axis than another, which should be an angle of about 30
        // degrees from the axis

        if (absX > 2.5 * absY)
          this.lockedY = sY;
        else if (absY > absX)
          this.lockedX = sX;

        this.locked = true;
      }
    }

    // Never lock if the dragger requests it
    if (this.alwaysFreeDrag) {
      this.lockedY = null;
      this.lockedX = null;
    }

    // After pan lock, figure out previous panning position. Base it on last drag
    // position so there isn't a jump in panning.
    let [prevX, prevY] = this._lockAxis(this.sX, this.sY);
    this.prevPanX = prevX;
    this.prevPanY = prevY;

    this.sX = sX;
    this.sY = sY;
  },

  setDragStart: function setDragStart(screenX, screenY, aDraggable) {
    this.sX = this._originX = screenX;
    this.sY = this._originY = screenY;
    this.dragging = true;

    // If the target area is pannable only in one direction lock it early
    // on the right axis
    this.lockedX = !aDraggable.x ? screenX : null;
    this.lockedY = !aDraggable.y ? screenY : null;
    this.stayLocked = this.lockedX || this.lockedY;
    this.locked = this.stayLocked;
  },

  endDrag: function endDrag() {
    this._resetActive();
    this.dragging = false;
  },

  /** Returns true if drag should pan scrollboxes.*/
  isPan: function isPan() {
    return this._isPan;
  },

  /** Return true if drag should be parsed as a click. */
  isClick: function isClick() {
    return !this._isPan;
  },

  /**
   * Returns the screen position for a pan. This factors in axis locking.
   * @return Array of screen X and Y coordinates
   */
  panPosition: function panPosition() {
    return this._lockAxis(this.sX, this.sY);
  },

  /** dismiss the active state of the pan element */
  _resetActive: function _resetActive() {
    let target = document.documentElement;
    // If the target is active, toggle (turn off) the active flag. Otherwise do nothing.
    if (this._domUtils.getContentState(target) & kStateActive)
      this._domUtils.setContentState(target, kStateActive);
  },

  toString: function toString() {
    return '[DragData] { sX,sY=' + this.sX + ',' + this.sY + ', dragging=' + this.dragging + ' }';
  }
};


/**
 * KineticController - a class to take drag position data and use it
 * to do kinetic panning of a scrollable object.
 *
 * aPanBy is a function that will be called with the dx and dy
 * generated by the kinetic algorithm.  It should return true if the
 * object was panned, false if there was no movement.
 *
 * There are two complicated things done here.  One is calculating the
 * initial velocity of the movement based on user input.  Two is
 * calculating the distance to move every frame.
 */
function KineticController(aPanBy, aEndCallback) {
  this._panBy = aPanBy;
  this._beforeEnd = aEndCallback;

  // These are used to calculate the position of the scroll panes during kinetic panning. Think of
  // these points as vectors that are added together and multiplied by scalars.
  this._position = new Point(0, 0);
  this._velocity = new Point(0, 0);
  this._acceleration = new Point(0, 0);
  this._time = 0;
  this._timeStart = 0;

  // How often do we change the position of the scroll pane?  Too often and panning may jerk near
  // the end. Too little and panning will be choppy. In milliseconds.
  this._updateInterval = Services.prefs.getIntPref("browser.ui.kinetic.updateInterval");
  // Constants that affect the "friction" of the scroll pane.
  this._exponentialC = Services.prefs.getIntPref("browser.ui.kinetic.exponentialC");
  this._polynomialC = Services.prefs.getIntPref("browser.ui.kinetic.polynomialC") / 1000000;
  // Number of milliseconds that can contain a swipe. Movements earlier than this are disregarded.
  this._swipeLength = Services.prefs.getIntPref("browser.ui.kinetic.swipeLength");

  this._reset();
}

KineticController.prototype = {
  _reset: function _reset() {
    this._active = false;
    this._paused = false;
    this.momentumBuffer = [];
    this._velocity.set(0, 0);
  },

  isActive: function isActive() {
    return this._active;
  },

  _startTimer: function _startTimer() {
    let self = this;

    let lastp = this._position;  // track last position vector because pan takes deltas
    let v0 = this._velocity;  // initial velocity
    let a = this._acceleration;  // acceleration
    let c = this._exponentialC;
    let p = new Point(0, 0);
    let dx, dy, t, realt;

    function calcP(v0, a, t) {
      // Important traits for this function:
      //   p(t=0) is 0
      //   p'(t=0) is v0
      //
      // We use exponential to get a smoother stop, but by itself exponential
      // is too smooth at the end. Adding a polynomial with the appropriate
      // weight helps to balance
      return v0 * Math.exp(-t / c) * -c + a * t * t + v0 * c;
    }

    this._calcV = function(v0, a, t) {
      return v0 * Math.exp(-t / c) + 2 * a * t;
    }

    let callback = {
      sample: function kineticHandleEvent(timeStamp) {
        // Someone called end() on us between timer intervals
        // or we are paused.
        if (!self.isActive() || self._paused)
          return;

        // To make animation end fast enough but to keep smoothness, average the ideal
        // time frame (smooth animation) with the actual time lapse (end fast enough).
        // Animation will never take longer than 2 times the ideal length of time.
        realt = timeStamp - self._initialTime;
        self._time += self._updateInterval;
        t = (self._time + realt) / 2;

        // Calculate new position.
        p.x = calcP(v0.x, a.x, t);
        p.y = calcP(v0.y, a.y, t);
        dx = Math.round(p.x - lastp.x);
        dy = Math.round(p.y - lastp.y);

        // Test to see if movement is finished for each component.
        if (dx * a.x > 0) {
          dx = 0;
          lastp.x = 0;
          v0.x = 0;
          a.x = 0;
        }
        // Symmetric to above case.
        if (dy * a.y > 0) {
          dy = 0;
          lastp.y = 0;
          v0.y = 0;
          a.y = 0;
        }

        if (v0.x == 0 && v0.y == 0) {
          self.end();
        } else {
          let panStop = false;
          if (dx != 0 || dy != 0) {
            try { panStop = !self._panBy(-dx, -dy, true); } catch (e) {}
            lastp.add(dx, dy);
          }

          if (panStop)
            self.end();
          else
            mozRequestAnimationFrame(this);
        }
      }
    };

    this._active = true;
    this._paused = false;
    mozRequestAnimationFrame(callback);
  },

  start: function start() {
    function sign(x) {
      return x ? ((x > 0) ? 1 : -1) : 0;
    }

    function clampFromZero(x, closerToZero, furtherFromZero) {
      if (x >= 0)
        return Math.max(closerToZero, Math.min(furtherFromZero, x));
      return Math.min(-closerToZero, Math.max(-furtherFromZero, x));
    }

    let mb = this.momentumBuffer;
    let mblen = this.momentumBuffer.length;

    let lastTime = mb[mblen - 1].t;
    let distanceX = 0;
    let distanceY = 0;
    let swipeLength = this._swipeLength;

    // determine speed based on recorded input
    let me;
    for (let i = 0; i < mblen; i++) {
      me = mb[i];
      if (lastTime - me.t < swipeLength) {
        distanceX += me.dx;
        distanceY += me.dy;
      }
    }

    let currentVelocityX = 0;
    let currentVelocityY = 0;

    if (this.isActive()) {
      // If active, then we expect this._calcV to be defined.
      let currentTime = Date.now() - this._initialTime;
      currentVelocityX = Util.clamp(this._calcV(this._velocity.x, this._acceleration.x, currentTime), -kMaxVelocity, kMaxVelocity);
      currentVelocityY = Util.clamp(this._calcV(this._velocity.y, this._acceleration.y, currentTime), -kMaxVelocity, kMaxVelocity);
    }

    if (currentVelocityX * this._velocity.x <= 0)
      currentVelocityX = 0;
    if (currentVelocityY * this._velocity.y <= 0)
      currentVelocityY = 0;

    let swipeTime = Math.min(swipeLength, lastTime - mb[0].t);
    this._velocity.x = clampFromZero((distanceX / swipeTime) + currentVelocityX, Math.abs(currentVelocityX), kMaxVelocity);
    this._velocity.y = clampFromZero((distanceY / swipeTime) + currentVelocityY, Math.abs(currentVelocityY), kMaxVelocity);

    if (Math.abs(this._velocity.x) < kMinVelocity)
      this._velocity.x = 0;
    if (Math.abs(this._velocity.y) < kMinVelocity)
      this._velocity.y = 0;

    // Set acceleration vector to opposite signs of velocity
    this._acceleration.set(this._velocity.clone().map(sign).scale(-this._polynomialC));

    this._position.set(0, 0);
    this._initialTime = mozAnimationStartTime;
    this._time = 0;
    this.momentumBuffer = [];

    if (!this.isActive() || this._paused)
      this._startTimer();

    return true;
  },

  pause: function pause() {
    this._paused = true;
  },

  end: function end() {
    if (this.isActive()) {
      if (this._beforeEnd)
        this._beforeEnd();
      this._reset();
    }
  },

  addData: function addData(dx, dy) {
    let mbLength = this.momentumBuffer.length;
    let now = Date.now();

    if (this.isActive()) {
      // Stop active movement when dragging in other direction.
      if (dx * this._velocity.x < 0 || dy * this._velocity.y < 0)
        this.end();
    }

    this.momentumBuffer.push({'t': now, 'dx' : dx, 'dy' : dy});
  }
};


/**
 * Input module for basic scrollwheel input.  Currently just zooms the browser
 * view accordingly.
 */
var ScrollwheelModule = {
  _pendingEvent : 0,
  _container: null,
  
  init: function init(container) {
    this._container = container;
    window.addEventListener("MozPrecisePointer", this, true);
    window.addEventListener("MozImprecisePointer", this, true);
  },

  handleEvent: function handleEvent(aEvent) {
    switch(aEvent.type) {
      case "DOMMouseScroll":
      case "MozMousePixelScroll":
        this._onScroll(aEvent);
        break;
      case "MozPrecisePointer":
        this._container.removeEventListener("DOMMouseScroll", this, true);
        this._container.removeEventListener("MozMousePixelScroll", this, true);
        break;
      case "MozImprecisePointer":
        this._container.addEventListener("DOMMouseScroll", this, true);
        this._container.addEventListener("MozMousePixelScroll", this, true);
        break;
    };
  },

  _onScroll: function _onScroll(aEvent) {
    // If events come too fast we don't want their handling to lag the
    // zoom in/zoom out execution. With the timeout the zoom is executed
    // as we scroll.
    if (this._pendingEvent)
      clearTimeout(this._pendingEvent);

    this._pendingEvent = setTimeout(function handleEventImpl(self) {
      self._pendingEvent = 0;
      Browser.zoom(aEvent.detail);
    }, 0, this);

    aEvent.stopPropagation();
    aEvent.preventDefault();
  },

  /* We don't have much state to reset if we lose event focus */
  cancelPending: function cancelPending() {}
};


/*
 * Simple gestures support
 */

var GestureModule = {
  _debugEvents: false,

  init: function init() {
    window.addEventListener("MozSwipeGesture", this, true);
    /*
    window.addEventListener("MozMagnifyGestureStart", this, true);
    window.addEventListener("MozMagnifyGestureUpdate", this, true);
    window.addEventListener("MozMagnifyGesture", this, true);
    */
    window.addEventListener("CancelTouchSequence", this, true);
  },

  _initMouseEventFromGestureEvent: function _initMouseEventFromGestureEvent(aDestEvent, aSrcEvent, aType, aCanBubble, aCancellable) {
    aDestEvent.initMouseEvent(aType, aCanBubble, aCancellable, window, null,
                              aSrcEvent.screenX, aSrcEvent.screenY, aSrcEvent.clientX, aSrcEvent.clientY,
                              aSrcEvent.ctrlKey, aSrcEvent.altKey, aSrcEvent.shiftKey, aSrcEvent.metaKey,
                              aSrcEvent.button, aSrcEvent.relatedTarget);
  },

  /*
   * Events
   *
   * Dispatch events based on the type of mouse gesture event. For now, make
   * sure to stop propagation of every gesture event so that web content cannot
   * receive gesture events.
   *
   * @param nsIDOMEvent information structure
   */

  handleEvent: function handleEvent(aEvent) {
    try {
      aEvent.stopPropagation();
      aEvent.preventDefault();
      if (this._debugEvents) Util.dumpLn("GestureModule:", aEvent.type);
      switch (aEvent.type) {
        case "MozSwipeGesture":
          if (this._onSwipe(aEvent)) {
            let event = document.createEvent("Events");
            event.initEvent("CancelTouchSequence", true, true);
            aEvent.target.dispatchEvent(event);
          }
          break;

        // Magnify currently doesn't work for Win8 (bug 593168)
        /*
        case "MozMagnifyGestureStart":
          this._pinchStart(aEvent);
          break;

        case "MozMagnifyGestureUpdate":
          this._pinchUpdate(aEvent);
          break;

        case "MozMagnifyGesture":
          this._pinchEnd(aEvent);
          break;
        */

        case "CancelTouchSequence":
          this.cancelPending();
          break;
      }
    } catch (e) {
      Util.dumpLn("Error while handling gesture event", aEvent.type,
                  "\nPlease report error at:", e);
      Cu.reportError(e);
    }
  },

  /*
   * Event handlers
   */

  cancelPending: function cancelPending() {
    if (AnimatedZoom.isZooming())
      AnimatedZoom.finish();
  },

  _onSwipe: function _onSwipe(aEvent) {
    switch (aEvent.direction) {
      case Ci.nsIDOMSimpleGestureEvent.DIRECTION_LEFT:
        return this._tryCommand("cmd_forward");
      case Ci.nsIDOMSimpleGestureEvent.DIRECTION_RIGHT:
        return this._tryCommand("cmd_back");
    }
    return false;
  },

  _tryCommand: function _tryCommand(aId) {
     if (document.getElementById(aId).getAttribute("disabled") == "true")
       return false;
     CommandUpdater.doCommand(aId);
     return true;
  },

  _pinchStart: function _pinchStart(aEvent) {
    if (AnimatedZoom.isZooming())
      return;
    // Cancel other touch sequence events, and be courteous by allowing them
    // to say no.
    let event = document.createEvent("Events");
    event.initEvent("CancelTouchSequence", true, true);
    let success = aEvent.target.dispatchEvent(event);

    if (!success || !Browser.selectedTab.allowZoom)
      return;

    AnimatedZoom.start();
    this._pinchDelta = 0;

    //this._ignoreNextUpdate = true; // first update gives useless, huge delta

    // cache gesture limit values
    this._maxGrowth = Services.prefs.getIntPref("browser.ui.pinch.maxGrowth");
    this._maxShrink = Services.prefs.getIntPref("browser.ui.pinch.maxShrink");
    this._scalingFactor = Services.prefs.getIntPref("browser.ui.pinch.scalingFactor");

    // Adjust the client coordinates to be relative to the browser element's top left corner.
    this._browserBCR = getBrowser().getBoundingClientRect();
    this._pinchStartX = aEvent.clientX - this._browserBCR.left;
    this._pinchStartY = aEvent.clientY - this._browserBCR.top;
  },

  _pinchUpdate: function _pinchUpdate(aEvent) {
    if (!AnimatedZoom.isZooming() || !aEvent.delta)
      return;

    let delta = 0;
    let browser = AnimatedZoom.browser;
    let oldScale = browser.scale;
    let bcr = this._browserBCR;

    // Accumulate pinch delta. Small changes are just jitter.
    this._pinchDelta += aEvent.delta;
    if (Math.abs(this._pinchDelta) >= oldScale) {
      delta = this._pinchDelta;
      this._pinchDelta = 0;
    }

    // decrease the pinchDelta min/max values to limit zooming out/in speed
    delta = Util.clamp(delta, -this._maxShrink, this._maxGrowth);

    let newScale = Browser.selectedTab.clampZoomLevel(oldScale * (1 + delta / this._scalingFactor));
    let startScale = AnimatedZoom.startScale;
    let scaleRatio = startScale / newScale;
    let cX = aEvent.clientX - bcr.left;
    let cY = aEvent.clientY - bcr.top;

    // Calculate the new zoom rect.
    let rect = AnimatedZoom.zoomFrom.clone();
    rect.translate(this._pinchStartX - cX + (1-scaleRatio) * cX * rect.width / bcr.width,
                   this._pinchStartY - cY + (1-scaleRatio) * cY * rect.height / bcr.height);

    rect.width *= scaleRatio;
    rect.height *= scaleRatio;

    this.translateInside(rect, new Rect(0, 0, browser.contentDocumentWidth * startScale,
                                              browser.contentDocumentHeight * startScale));

    // redraw zoom canvas according to new zoom rect
    AnimatedZoom.updateTo(rect);
  },

  _pinchEnd: function _pinchEnd(aEvent) {
    if (AnimatedZoom.isZooming())
      AnimatedZoom.finish();
  },

  /**
   * Ensure r0 is inside r1, if possible. Preserves w, h.
   * Same as Rect.prototype.translateInside except it aligns the top left
   * instead of the bottom right if r0 is bigger than r1.
   */
  translateInside: function translateInside(r0, r1) {
    let offsetX = (r0.left < r1.left ? r1.left - r0.left :
        (r0.right > r1.right ? Math.max(r1.left - r0.left, r1.right - r0.right) : 0));
    let offsetY = (r0.top < r1.top ? r1.top - r0.top :
        (r0.bottom > r1.bottom ? Math.max(r1.top - r0.top, r1.bottom - r0.bottom) : 0));
    return r0.translate(offsetX, offsetY);
  }
};

/**
 * Helper to track when the user is using a precise pointing device (pen/mouse)
 * versus an imprecise one (touch).
 */
var InputSourceHelper = {
  isPrecise: false,
  treatMouseAsTouch: false,

  init: function ish_init() {
    // debug feature, make all input imprecise
    try {
      this.treatMouseAsTouch = Services.prefs.getBoolPref(kDebugMouseInputPref);
    } catch (e) {}
    if (!this.treatMouseAsTouch) {
      window.addEventListener("mousemove", this, true);
      window.addEventListener("mousedown", this, true);
    }
  },
  
  handleEvent: function ish_handleEvent(aEvent) {
    switch (aEvent.mozInputSource) {
      case Ci.nsIDOMMouseEvent.MOZ_SOURCE_MOUSE:
      case Ci.nsIDOMMouseEvent.MOZ_SOURCE_PEN:
      case Ci.nsIDOMMouseEvent.MOZ_SOURCE_ERASER:
      case Ci.nsIDOMMouseEvent.MOZ_SOURCE_CURSOR:
        if (!this.isPrecise && !this.treatMouseAsTouch) {
          this.isPrecise = true;
          this._fire("MozPrecisePointer");
        }
        break;

      case Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH:
        if (this.isPrecise) {
          this.isPrecise = false;
          this._fire("MozImprecisePointer");
        }
        break;
    }
  },
  
  fireUpdate: function fireUpdate() {
    if (this.treatMouseAsTouch) {
      this._fire("MozImprecisePointer");
    } else {
      if (this.isPrecise) {
        this._fire("MozPrecisePointer");
      } else {
        this._fire("MozImprecisePointer");
      }
    }
  },

  _fire: function (name) {
    let event = document.createEvent("Events");
    event.initEvent(name, true, true);
    window.dispatchEvent(event);
  }
};
