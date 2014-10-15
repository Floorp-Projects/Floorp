'use strict';

/* global getMainChromeWindow, AccessFuTest, GestureSettings, GestureTracker,
   SimpleTest, getBoundsForDOMElm, Point, Utils */
/* exported loadJSON, eventMap */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import('resource://gre/modules/Geometry.jsm');
Cu.import("resource://gre/modules/accessibility/Gestures.jsm");

var win = getMainChromeWindow(window);

/**
 * Convert inch based point coordinates into pixels.
 * @param  {Array} aPoints Array of coordinates in inches.
 * @return {Array} Array of coordinates in pixels.
 */
function convertPointCoordinates(aPoints) {
  var dpi = Utils.dpi;
  return aPoints.map(function convert(aPoint) {
    return {
      x: aPoint.x * dpi,
      y: aPoint.y * dpi,
      identifier: aPoint.identifier
    };
  });
}

/**
 * For a given list of points calculate their coordinates in relation to the
 * document body.
 * @param  {Array} aTouchPoints An array of objects of the following format: {
 *   base: {String}, // Id of an element to server as a base for the touch.
 *   x: {Number},    // An optional x offset from the base element's geometric
 *                   // centre.
 *   y: {Number}     // An optional y offset from the base element's geometric
 *                   // centre.
 * }
 * @return {JSON} An array of {x, y} coordinations.
 */
function calculateTouchListCoordinates(aTouchPoints) {
  var coords = [];
  for (var i = 0, target = aTouchPoints[i]; i < aTouchPoints.length; ++i) {
    var bounds = getBoundsForDOMElm(target.base);
    var parentBounds = getBoundsForDOMElm('root');
    var point = new Point(target.x || 0, target.y || 0);
    point.scale(Utils.dpi);
    point.add(bounds[0], bounds[1]);
    point.add(bounds[2] / 2, bounds[3] / 2);
    point.subtract(parentBounds[0], parentBounds[0]);
    coords.push({
      x: point.x,
      y: point.y
    });
  }
  return coords;
}

/**
 * Send a touch event with specified touchPoints.
 * @param  {Array} aTouchPoints An array of points to be associated with
 * touches.
 * @param  {String} aName A name of the touch event.
 */
function sendTouchEvent(aTouchPoints, aName) {
  var touchList = sendTouchEvent.touchList;
  if (aName === 'touchend') {
    sendTouchEvent.touchList = null;
  } else {
    var coords = calculateTouchListCoordinates(aTouchPoints);
    var touches = [];
    for (var i = 0; i < coords.length; ++i) {
      var {x, y} = coords[i];
      var node = document.elementFromPoint(x, y);
      var touch = document.createTouch(window, node, aName === 'touchstart' ?
        1 : touchList.item(i).identifier, x, y, x, y);
      touches.push(touch);
    }
    touchList = document.createTouchList(touches);
    sendTouchEvent.touchList = touchList;
  }
  var evt = document.createEvent('TouchEvent');
  evt.initTouchEvent(aName, true, true, window, 0, false, false, false, false,
    touchList, touchList, touchList);
  document.dispatchEvent(evt);
}

sendTouchEvent.touchList = null;

/**
 * A map of event names to the functions that actually send them.
 * @type {Object}
 */
var eventMap = {
  touchstart: sendTouchEvent,
  touchend: sendTouchEvent,
  touchmove: sendTouchEvent
};

var originalDwellThreshold = GestureSettings.dwellThreshold;
var originalSwipeMaxDuration = GestureSettings.swipeMaxDuration;

/**
 * Attach a listener for the mozAccessFuGesture event that tests its
 * type.
 * @param  {Array} aExpectedGestures A stack of expected event types.
 * Note: the listener is removed once the stack reaches 0.
 */
function testMozAccessFuGesture(aExpectedGestures) {
  var types = aExpectedGestures;
  function handleGesture(aEvent) {
    if (aEvent.detail.type !== types[0].type) {
      // The is not the event of interest.
      return;
    }
    is(!!aEvent.detail.edge, !!types[0].edge);
    ok(true, 'Received correct mozAccessFuGesture: ' + JSON.stringify(types.shift()) + '.');
    if (types.length === 0) {
      win.removeEventListener('mozAccessFuGesture', handleGesture);
      if (AccessFuTest.sequenceCleanup) {
        AccessFuTest.sequenceCleanup();
      }
      AccessFuTest.nextTest();
    }
  }
  win.addEventListener('mozAccessFuGesture', handleGesture);
}

/**
 * Reset the thresholds and max delays that affect gesture rejection.
 * @param {Number} aTimeStamp Gesture time stamp.
 * @param {Boolean} aRemoveDwellThreshold An optional flag to reset dwell
 * threshold.
 * @param {Boolean} aRemoveSwipeMaxDuration An optional flag to reset swipe max
 * duration.
 */
function setTimers(aTimeStamp, aRemoveDwellThreshold, aRemoveSwipeMaxDuration) {
  if (!aRemoveDwellThreshold && !aRemoveSwipeMaxDuration) {
    return;
  }
  if (aRemoveDwellThreshold) {
    GestureSettings.dwellThreshold = 0;
  }
  if (aRemoveSwipeMaxDuration) {
    GestureSettings.swipeMaxDuration = 0;
  }
  GestureTracker.current.clearTimer();
  GestureTracker.current.startTimer(aTimeStamp);
}

function resetTimers() {
  GestureSettings.dwellThreshold = originalDwellThreshold;
  GestureSettings.swipeMaxDuration = originalSwipeMaxDuration;
}

/**
 * An extention to AccessFuTest that adds an ability to test a sequence of
 * pointer events and their expected mozAccessFuGesture events.
 * @param {Object} aSequence An object that has a list of pointer events to be
 * generated and the expected mozAccessFuGesture events.
 */
AccessFuTest.addSequence = function AccessFuTest_addSequence(aSequence) {
  AccessFuTest.addFunc(function testSequence() {
    testMozAccessFuGesture(aSequence.expectedGestures);
    var events = aSequence.events;
    function fireEvent(aEvent) {
      var event = {
        points: convertPointCoordinates(aEvent.points),
        type: aEvent.type
      };
      var timeStamp = Date.now();
      resetTimers();
      GestureTracker.handle(event, timeStamp);
      setTimers(timeStamp, aEvent.removeDwellThreshold,
        aEvent.removeSwipeMaxDuration);
      processEvents();
    }
    function processEvents() {
      if (events.length === 0) {
        return;
      }
      var event = events.shift();
      SimpleTest.executeSoon(function() {
        fireEvent(event);
      });
    }
    processEvents();
  });
};

/**
 * A helper function that loads JSON files.
 * @param {String} aPath A path to a JSON file.
 * @param {Function} aCallback A callback to be called on success.
 */
function loadJSON(aPath, aCallback) {
  var request = new XMLHttpRequest();
  request.open('GET', aPath, true);
  request.responseType = 'json';
  request.onload = function onload() {
    aCallback(request.response);
  };
  request.send();
}
