/**
 * web-audio-automation-timeline - 1.0.3
 * https://github.com/jsantell/web-audio-automation-timeline
 * MIT License, copyright (c) 2014 Jordan Santell
 */
!function(e){if("object"==typeof exports&&"undefined"!=typeof module)module.exports=e();else if("function"==typeof define&&define.amd)define([],e);else{var f;"undefined"!=typeof window?f=window:"undefined"!=typeof global?f=global:"undefined"!=typeof self&&(f=self),f.Timeline=e()}}(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
module.exports = require("./lib/timeline").Timeline;

},{"./lib/timeline":4}],2:[function(require,module,exports){
var F = require("./formulas");

function TimelineEvent (eventName, value, time, timeConstant, duration) {
  this.type = eventName;
  this.value = value;
  this.time = time;
  this.constant = timeConstant || 0;
  this.duration = duration || 0;
}
exports.TimelineEvent = TimelineEvent;


TimelineEvent.prototype.exponentialApproach = function (lastValue, time) {
  return F.exponentialApproach(this.time, lastValue, this.value, this.constant, time);
}

TimelineEvent.prototype.extractValueFromCurve = function (time) {
  return F.extractValueFromCurve(this.time, this.value, this.value.length, this.duration, time);
}

TimelineEvent.prototype.linearInterpolate = function (next, time) {
  return F.linearInterpolate(this.time, this.value, next.time, next.value, time);
}

TimelineEvent.prototype.exponentialInterpolate = function (next, time) {
  return F.exponentialInterpolate(this.time, this.value, next.time, next.value, time);
}

},{"./formulas":3}],3:[function(require,module,exports){
var EPSILON = 0.0000000001;

exports.linearInterpolate = function (t0, v0, t1, v1, t) {
  return v0 + (v1 - v0) * ((t - t0) / (t1 - t0));
};

exports.exponentialInterpolate = function (t0, v0, t1, v1, t) {
  return v0 * Math.pow(v1 / v0, (t - t0) / (t1 - t0));
};

exports.extractValueFromCurve = function (start, curve, curveLength, duration, t) {
  var ratio;

  // If time is after duration, return the last curve value,
  // or if ratio is >= 1
  if (t >= start + duration || (ratio = Math.max((t - start) / duration, 0)) >= 1) {
    return curve[curveLength - 1];
  }

  return curve[~~(curveLength * ratio)];
};

exports.exponentialApproach = function (t0, v0, v1, timeConstant, t) {
  return v1 + (v0 - v1) * Math.exp(-(t - t0) / timeConstant);
};

// Since we are going to accumulate error by adding 0.01 multiple times
// in a loop, we want to fuzz the equality check in `getValueAtTime`
exports.fuzzyEqual = function (lhs, rhs) {
  return Math.abs(lhs - rhs) < EPSILON;
};

exports.EPSILON = EPSILON;

},{}],4:[function(require,module,exports){
var TimelineEvent = require("./event").TimelineEvent;
var F = require("./formulas");

exports.Timeline = Timeline;

function Timeline (defaultValue) {
  this.events = [];

  this._value = defaultValue || 0;
}

Timeline.prototype.getEventCount = function () {
  return this.events.length;
};

Timeline.prototype.value = function () {
  return this._value;
};

Timeline.prototype.setValue = function (value) {
  if (this.events.length === 0) {
    this._value = value;
  }
};

Timeline.prototype.getValue = function () {
  if (this.events.length) {
    throw new Error("Can only call `getValue` when there are 0 events.");
  }

  return this._value;
};

Timeline.prototype.getValueAtTime = function (time) {
  return this._getValueAtTimeHelper(time);
};

Timeline.prototype._getValueAtTimeHelper = function (time) {
  var bailOut = false;
  var previous = null;
  var next = null;
  var lastComputedValue = null; // Used for `setTargetAtTime` nodes
  var events = this.events;
  var e;

  for (var i = 0; !bailOut && i < events.length; i++) {
    if (F.fuzzyEqual(time, events[i].time)) {
      // Find the last event with the same time as `time`
      do {
        ++i;
      } while (i < events.length && F.fuzzyEqual(time, events[i].time));

      e = events[i - 1];

      // `setTargetAtTime` can be handled no matter what their next event is (if they have one)
      if (e.type === "setTargetAtTime") {
        lastComputedValue = this._lastComputedValue(e);
        return e.exponentialApproach(lastComputedValue, time);
      }

      // `setValueCurveAtTime` events can be handled no matter what their next event node is
      // (if they have one)
      if (e.type === "setValueCurveAtTime") {
        return e.extractValueFromCurve(time);
      }

      // For other event types
      return e.value;
    }
    previous = next;
    next = events[i];

    if (time < events[i].time) {
      bailOut = true;
    }
  }

  // Handle the case where the time is past all of the events
  if (!bailOut) {
    previous = next;
    next = null;
  }

  // Just return the default value if we did not find anything
  if (!previous && !next) {
    return this._value;
  }

  // If the requested time is before all of the existing events
  if (!previous) {
    return this._value;
  }

  // `setTargetAtTime` can be handled no matter what their next event is (if they have one)
  if (previous.type === "setTargetAtTime") {
    lastComputedValue = this._lastComputedValue(previous);
    return previous.exponentialApproach(lastComputedValue, time);
  }

  // `setValueCurveAtTime` events can be handled no matter what their next event node is
  // (if they have one)
  if (previous.type === "setValueCurveAtTime") {
    return previous.extractValueFromCurve(time);
  }

  if (!next) {
    if (~["setValueAtTime", "linearRampToValueAtTime", "exponentialRampToValueAtTime"].indexOf(previous.type)) {
      return previous.value;
    }
    if (previous.type === "setValueCurveAtTime") {
      return previous.extractValueFromCurve(time);
    }
    if (previous.type === "setTargetAtTime") {
      throw new Error("unreached");
    }
    throw new Error("unreached");
  }

  // Finally handle the case where we have both a previous and a next event
  // First handle the case where our range ends up in a ramp event
  if (next.type === "linearRampToValueAtTime") {
    return previous.linearInterpolate(next, time);
  } else if (next.type === "exponentialRampToValueAtTime") {
    return previous.exponentialInterpolate(next, time);
  }

  // Now handle all other cases
  if (~["setValueAtTime", "linearRampToValueAtTime", "exponentialRampToValueAtTime"].indexOf(previous.type)) {
    // If the next event type is neither linear or exponential ramp,
    // the value is constant.
    return previous.value;
  }
  if (previous.type === "setValueCurveAtTime") {
    return previous.extractValueFromCurve(time);
  }
  if (previous.type === "setTargetAtTime") {
    throw new Error("unreached");
  }
  throw new Error("unreached");
};

Timeline.prototype._insertEvent = function (ev) {
  var events = this.events;

  if (ev.type === "setValueCurveAtTime") {
    if (!ev.value || !ev.value.length) {
      throw new Error("NS_ERROR_DOM_SYNTAX_ERR");
    }
  }

  if (ev.type === "setTargetAtTime") {
    if (F.fuzzyEqual(ev.constant, 0)) {
      throw new Error("NS_ERROR_DOM_SYNTAX_ERR");
    }
  }

  // Make sure that non-curve events don't fall within the duration of a
  // curve event.
  for (var i = 0; i < events.length; i++) {
    if (events[i].type === "setValueCurveAtTime" &&
        events[i].time <= ev.time &&
        (events[i].time + events[i].duration) >= ev.time) {
      throw new Error("NS_ERROR_DOM_SYNTAX_ERR");
    }
  }

  // Make sure that curve events don't fall in a range which includes other
  // events.
  if (ev.type === "setValueCurveAtTime") {
    for (var i = 0; i < events.length; i++) {
      if (events[i].time > ev.time &&
          events[i].time < (ev.time + ev.duration)) {
        throw new Error("NS_ERROR_DOM_SYNTAX_ERR");
      }
    }
  }

  // Make sure that invalid values are not used for exponential curves
  if (ev.type === "exponentialRampToValueAtTime") {
    if (ev.value <= 0) throw new Error("NS_ERROR_DOM_SYNTAX_ERR");
    var prev = this._getPreviousEvent(ev.time);
    if (prev) {
      if (prev.value <= 0) {
        throw new Error("NS_ERROR_DOM_SYNTAX_ERR");
      }
    } else {
      if (this._value <= 0) {
        throw new Error("NS_ERROR_DOM_SYNTAX_ERR");
      }
    }
  }

  for (var i = 0; i < events.length; i++) {
    if (ev.time === events[i].time) {
      if (ev.type === events[i].type) {
        // If times and types are equal, replace the event;
        events[i] = ev;
      } else {
        // Otherwise, place the element after the last event of another type
        do { i++; }
        while (i < events.length && ev.type !== events[i].type && ev.time === events[i].time);
        events.splice(i, 0, ev);
      }
      return;
    }
    // Otherwise, place the event right after the latest existing event
    if (ev.time < events[i].time) {
      events.splice(i, 0, ev);
      return;
    }
  }

  // If we couldn't find a place for the event, just append it to the list
  this.events.push(ev);
};

Timeline.prototype._getPreviousEvent = function (time) {
  var previous = null, next = null;
  var bailOut = false;
  var events = this.events;

  for (var i = 0; !bailOut && i < events.length; i++) {
    if (time === events[i]) {
      do { ++i; }
      while (i < events.length && time === events[i].time);
      return events[i - 1];
    }
    previous = next;
    next = events[i];
    if (time < events[i].time) {
      bailOut = true;
    }
  }

  // Handle the case where the time is past all the events
  if (!bailOut) {
    previous = next;
  }

  return previous;
};

/**
 * Calculates the previous value of the timeline, used for
 * `setTargetAtTime` nodes. Takes an event, and returns
 * the previous computed value for any sample taken during that
 * exponential approach node.
 */
Timeline.prototype._lastComputedValue = function (event) {
  // If equal times, return the value for the previous event, before
  // the `setTargetAtTime` node.
  var lastEvent = this._getPreviousEvent(event.time - F.EPSILON);

  // If no event before the setTargetAtTime event, then return the
  // intrinsic value.
  if (!lastEvent) {
    return this._value;
  }
  // Otherwise, return the value for the previous event, which should
  // always be the last computed value (? I think?)
  else {
    return lastEvent.value;
  }
};

Timeline.prototype.setValueAtTime = function (value, startTime) {
  this._insertEvent(new TimelineEvent("setValueAtTime", value, startTime));
};

Timeline.prototype.linearRampToValueAtTime = function (value, endTime) {
  this._insertEvent(new TimelineEvent("linearRampToValueAtTime", value, endTime));
};

Timeline.prototype.exponentialRampToValueAtTime = function (value, endTime) {
  this._insertEvent(new TimelineEvent("exponentialRampToValueAtTime", value, endTime));
};

Timeline.prototype.setTargetAtTime = function (value, startTime, timeConstant) {
  this._insertEvent(new TimelineEvent("setTargetAtTime", value, startTime, timeConstant));
};

Timeline.prototype.setValueCurveAtTime = function (value, startTime, duration) {
  this._insertEvent(new TimelineEvent("setValueCurveAtTime", value, startTime, null, duration));
};

Timeline.prototype.cancelScheduledValues = function (time) {
  for (var i = 0; i < this.events.length; i++) {
    if (this.events[i].time >= time) {
      this.events = this.events.slice(0, i);
      break;
    }
  }
};

Timeline.prototype.cancelAllEvents = function () {
  this.events.length = 0;
};

},{"./event":2,"./formulas":3}]},{},[1])(1)
});