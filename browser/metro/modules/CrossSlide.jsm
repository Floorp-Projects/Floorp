/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["CrossSlide"];

// needs DPI adjustment?
let CrossSlideThresholds = {
   SELECTIONSTART: 25,
   SPEEDBUMPSTART: 30,
   SPEEDBUMPEND: 50,
   REARRANGESTART: 80
};

// see: http://msdn.microsoft.com/en-us/library/windows/apps/windows.ui.input.crossslidingstate.ASPx
let CrossSlidingState = {
  STARTED:  0,
  DRAGGING: 1,
  SELECTING: 2,
  SELECT_SPEED_BUMPING: 3,
  SPEED_BUMPING: 4,
  REARRANGING: 5,
  COMPLETED: 6
};

let CrossSlidingStateNames = [
  'started',
  'dragging',
  'selecting',
  'selectSpeedBumping',
  'speedBumping',
  'rearranging',
  'completed'
];

// --------------------------------
// module helpers
//

function isSelectable(aElement) {
  // placeholder logic
  return aElement.nodeName == 'richgriditem';
}
function withinCone(aLen, aHeight) {
  // check pt falls within 45deg either side of the cross axis
  return aLen > aHeight;
}
function getScrollAxisFromElement(aElement) {
  // keeping it simple - just return apparent scroll axis for the document
  let win = aElement.ownerDocument.defaultView;
  let scrollX = win.scrollMaxX,
      scrollY = win.scrollMaxY;
  // determine scroll axis from scrollable content when possible
  if (scrollX || scrollY)
    return scrollX >= scrollY ? 'x' : 'y';

  // fall back to guessing at scroll axis from document aspect ratio
  let docElem = aElement.ownerDocument.documentElement;
  return  docElem.clientWidth >= docElem.clientHeight ?
          'x' : 'y';
}
function pointFromTouchEvent(aEvent) {
  let touch = aEvent.touches[0];
  return { x: touch.clientX, y: touch.clientY };
}
// This damping function has these important properties:
// f(0) = 0
// f'(0) = 1
// limit as x -> Infinity of f(x) = 1
function damp(aX) {
  return 2 / (1 + Math.exp(-2 * aX)) - 1;
}
function speedbump(aDelta, aStart, aEnd) {
  let x = Math.abs(aDelta);
  if (x <= aStart)
    return aDelta;
  let sign = aDelta / x;

  let d = aEnd - aStart;
  let damped = damp((x - aStart) / d);
  return sign * (aStart + (damped * d));
}


this.CrossSlide = {
  // -----------------------
  // Gesture constants
  Thresholds: CrossSlideThresholds,
  State: CrossSlidingState,
  StateNames: CrossSlidingStateNames,
  // -----------------------
  speedbump: speedbump
};

function CrossSlideHandler(aNode, aThresholds) {
  this.node = aNode;
  this.thresholds = Object.create(CrossSlideThresholds);
  // apply per-instance threshold configuration
  if (aThresholds) {
    for(let key in aThresholds)
      this.thresholds[key] = aThresholds[key];
  }
}

CrossSlideHandler.prototype = {
  node: null,
  drag: null,

  getCrossSlideState: function(aCrossAxisDistance, aScrollAxisDistance) {
    if (aCrossAxisDistance <= 0) {
      return CrossSlidingState.STARTED;
    }
    if (aCrossAxisDistance < this.thresholds.SELECTIONSTART) {
      return CrossSlidingState.DRAGGING;
    }
    if (aCrossAxisDistance < this.thresholds.SPEEDBUMPSTART) {
      return CrossSlidingState.SELECTING;
    }
    if (aCrossAxisDistance < this.thresholds.SPEEDBUMPEND) {
      return CrossSlidingState.SELECT_SPEED_BUMPING;
    }
    if (aCrossAxisDistance < this.thresholds.REARRANGESTART) {
      return CrossSlidingState.SPEED_BUMPING;
    }
    // out of bounds cross-slide
    return -1;
  },

  handleEvent: function handleEvent(aEvent) {
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
    }
  },

  cancel: function(aEvent){
    this._fireProgressEvent("cancelled", aEvent);
    this.drag = null;
  },

  _onTouchStart: function onTouchStart(aEvent){
    if (aEvent.touches.length > 1)
      return;
    let touch = aEvent.touches[0];
     // cross-slide is a single touch gesture
     // the top target is the one we need here, touch.target not relevant
    let target = aEvent.target;

    if (!isSelectable(target))
        return;

    let scrollAxis = getScrollAxisFromElement(target);

    this.drag = {
      scrollAxis: scrollAxis,
      crossAxis: (scrollAxis=='x') ? 'y' : 'x',
      origin: pointFromTouchEvent(aEvent),
      state: -1
    };
  },

  _onTouchMove: function(aEvent){
    if (!this.drag) {
      return;
    }

    if (aEvent.touches.length!==1) {
      // cancel if another touch point gets involved
      return this.cancel(aEvent);
    }

    let startPt = this.drag.origin;
    let endPt = this.drag.position = pointFromTouchEvent(aEvent);

    let scrollAxis = this.drag.scrollAxis,
        crossAxis = this.drag.crossAxis;

    // distance from the origin along the axis perpendicular to scrolling
    let crossAxisDistance = Math.abs(endPt[crossAxis] - startPt[crossAxis]);
    // distance along the scrolling axis
    let scrollAxisDistance = Math.abs(endPt[scrollAxis] - startPt[scrollAxis]);
    let currState = this.drag.state;
    let newState = this.getCrossSlideState(crossAxisDistance, scrollAxisDistance);

    switch (newState) {
      case -1 :
        // dodgy input/out of bounds
        return this.cancel(aEvent);
      case CrossSlidingState.STARTED :
        break;
      case CrossSlidingState.DRAGGING :
        if (scrollAxisDistance > this.thresholds.SELECTIONSTART) {
          // looks like a pan/scroll was intended
          return this.cancel(aEvent);
        }
        // else fall-thru'
      case CrossSlidingState.SELECTING :
      case CrossSlidingState.SELECT_SPEED_BUMPING :
      case CrossSlidingState.SPEED_BUMPING :
        // we're committed to a cross-slide gesture,
        // so going out of bounds at this point means aborting
        if (!withinCone(crossAxisDistance, scrollAxisDistance)) {
          return this.cancel(aEvent);
        }
        // we're mid-gesture, consume this event
        aEvent.stopPropagation();
        break;
    }

    if (currState !== newState) {
      this.drag.state = newState;
      this._fireProgressEvent( CrossSlidingStateNames[newState], aEvent );
    }
  },
  _onTouchEnd: function(aEvent){
    if (!this.drag)
      return;

    if (this.drag.state < CrossSlidingState.SELECTING) {
      return this.cancel(aEvent);
    }

    this._fireProgressEvent("completed", aEvent);
    this._fireSelectEvent(aEvent);
    this.drag = null;
  },

  /**
   * Dispatches a custom Event on the drag node.
   * @param aEvent The source event.
   * @param aType The event type.
   */
  _fireProgressEvent: function CrossSliding_fireEvent(aState, aEvent) {
    if (!this.drag)
        return;
    let event = this.node.ownerDocument.createEvent("Events");
    let crossAxis = this.drag.crossAxis;
    event.initEvent("MozCrossSliding", true, true);
    event.crossSlidingState = aState;
    event.position = this.drag.position;
    event.direction = this.drag.crossAxis;
    event.delta = this.drag.position[crossAxis] - this.drag.origin[crossAxis];
    aEvent.target.dispatchEvent(event);
  },

  /**
   * Dispatches a custom Event on the given target node.
   * @param aEvent The source event.
   */
  _fireSelectEvent: function SelectTarget_fireEvent(aEvent) {
    let event = this.node.ownerDocument.createEvent("Events");
    event.initEvent("MozCrossSlideSelect", true, true);
    event.position = this.drag.position;
    aEvent.target.dispatchEvent(event);
  }
};
this.CrossSlide.Handler = CrossSlideHandler;
