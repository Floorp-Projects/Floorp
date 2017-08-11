/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/old-event-emitter");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N =
      new LocalizationHelper("devtools/client/locales/animationinspector.properties");

// How many times, maximum, can we loop before we find the optimal time
// interval in the timeline graph.
const OPTIMAL_TIME_INTERVAL_MAX_ITERS = 100;
// Time graduations should be multiple of one of these number.
const OPTIMAL_TIME_INTERVAL_MULTIPLES = [1, 2.5, 5];

const MILLIS_TIME_FORMAT_MAX_DURATION = 4000;

// SVG namespace
const SVG_NS = "http://www.w3.org/2000/svg";

/**
 * DOM node creation helper function.
 * @param {Object} Options to customize the node to be created.
 * - nodeType {String} Optional, defaults to "div",
 * - attributes {Object} Optional attributes object like
 *   {attrName1:value1, attrName2: value2, ...}
 * - parent {DOMNode} Mandatory node to append the newly created node to.
 * - textContent {String} Optional text for the node.
 * - namespace {String} Optional namespace
 * @return {DOMNode} The newly created node.
 */
function createNode(options) {
  if (!options.parent) {
    throw new Error("Missing parent DOMNode to create new node");
  }

  let type = options.nodeType || "div";
  let node =
    options.namespace
    ? options.parent.ownerDocument.createElementNS(options.namespace, type)
    : options.parent.ownerDocument.createElement(type);

  for (let name in options.attributes || {}) {
    let value = options.attributes[name];
    node.setAttribute(name, value);
  }

  if (options.textContent) {
    node.textContent = options.textContent;
  }

  options.parent.appendChild(node);
  return node;
}

exports.createNode = createNode;

/**
 * SVG DOM node creation helper function.
 * @param {Object} Options to customize the node to be created.
 * - nodeType {String} Optional, defaults to "div",
 * - attributes {Object} Optional attributes object like
 *   {attrName1:value1, attrName2: value2, ...}
 * - parent {DOMNode} Mandatory node to append the newly created node to.
 * - textContent {String} Optional text for the node.
 * @return {DOMNode} The newly created node.
 */
function createSVGNode(options) {
  options.namespace = SVG_NS;
  return createNode(options);
}
exports.createSVGNode = createSVGNode;

/**
 * Find the optimal interval between time graduations in the animation timeline
 * graph based on a minimum time interval
 * @param {Number} minTimeInterval Minimum time in ms in one interval
 * @return {Number} The optimal interval time in ms
 */
function findOptimalTimeInterval(minTimeInterval) {
  let numIters = 0;
  let multiplier = 1;

  if (!minTimeInterval) {
    return 0;
  }

  let interval;
  while (true) {
    for (let i = 0; i < OPTIMAL_TIME_INTERVAL_MULTIPLES.length; i++) {
      interval = OPTIMAL_TIME_INTERVAL_MULTIPLES[i] * multiplier;
      if (minTimeInterval <= interval) {
        return interval;
      }
    }
    if (++numIters > OPTIMAL_TIME_INTERVAL_MAX_ITERS) {
      return interval;
    }
    multiplier *= 10;
  }
}

exports.findOptimalTimeInterval = findOptimalTimeInterval;

/**
 * Format a timestamp (in ms) as a mm:ss.mmm string.
 * @param {Number} time
 * @return {String}
 */
function formatStopwatchTime(time) {
  // Format falsy values as 0
  if (!time) {
    return "00:00.000";
  }

  let milliseconds = parseInt(time % 1000, 10);
  let seconds = parseInt((time / 1000) % 60, 10);
  let minutes = parseInt((time / (1000 * 60)), 10);

  let pad = (nb, max) => {
    if (nb < max) {
      return new Array((max + "").length - (nb + "").length + 1).join("0") + nb;
    }
    return nb;
  };

  minutes = pad(minutes, 10);
  seconds = pad(seconds, 10);
  milliseconds = pad(milliseconds, 100);

  return `${minutes}:${seconds}.${milliseconds}`;
}

exports.formatStopwatchTime = formatStopwatchTime;

/**
 * The TimeScale helper object is used to know which size should something be
 * displayed with in the animation panel, depending on the animations that are
 * currently displayed.
 * If there are 5 animations displayed, and the first one starts at 10000ms and
 * the last one ends at 20000ms, then this helper can be used to convert any
 * time in this range to a distance in pixels.
 *
 * For the helper to know how to convert, it needs to know all the animations.
 * Whenever a new animation is added to the panel, addAnimation(state) should be
 * called. reset() can be called to start over.
 */
var TimeScale = {
  minStartTime: Infinity,
  maxEndTime: 0,

  /**
   * Add a new animation to time scale.
   * @param {Object} state A PlayerFront.state object.
   */
  addAnimation: function (state) {
    let {previousStartTime, delay, duration, endDelay,
         iterationCount, playbackRate} = state;

    endDelay = typeof endDelay === "undefined" ? 0 : endDelay;
    let toRate = v => v / playbackRate;
    let minZero = v => Math.max(v, 0);
    let rateRelativeDuration =
      toRate(duration * (!iterationCount ? 1 : iterationCount));
    // Negative-delayed animations have their startTimes set such that we would
    // be displaying the delay outside the time window if we didn't take it into
    // account here.
    let relevantDelay = delay < 0 ? toRate(delay) : 0;
    previousStartTime = previousStartTime || 0;

    let startTime = toRate(minZero(delay)) +
                    rateRelativeDuration +
                    endDelay;
    this.minStartTime = Math.min(
      this.minStartTime,
      previousStartTime +
      relevantDelay +
      Math.min(startTime, 0)
    );
    let length = toRate(delay) +
                 rateRelativeDuration +
                 toRate(minZero(endDelay));
    let endTime = previousStartTime + length;
    this.maxEndTime = Math.max(this.maxEndTime, endTime);
  },

  /**
   * Reset the current time scale.
   */
  reset: function () {
    this.minStartTime = Infinity;
    this.maxEndTime = 0;
  },

  /**
   * Convert a startTime to a distance in %, in the current time scale.
   * @param {Number} time
   * @return {Number}
   */
  startTimeToDistance: function (time) {
    time -= this.minStartTime;
    return this.durationToDistance(time);
  },

  /**
   * Convert a duration to a distance in %, in the current time scale.
   * @param {Number} time
   * @return {Number}
   */
  durationToDistance: function (duration) {
    return duration * 100 / this.getDuration();
  },

  /**
   * Convert a distance in % to a time, in the current time scale.
   * @param {Number} distance
   * @return {Number}
   */
  distanceToTime: function (distance) {
    return this.minStartTime + (this.getDuration() * distance / 100);
  },

  /**
   * Convert a distance in % to a time, in the current time scale.
   * The time will be relative to the current minimum start time.
   * @param {Number} distance
   * @return {Number}
   */
  distanceToRelativeTime: function (distance) {
    let time = this.distanceToTime(distance);
    return time - this.minStartTime;
  },

  /**
   * Depending on the time scale, format the given time as milliseconds or
   * seconds.
   * @param {Number} time
   * @return {String} The formatted time string.
   */
  formatTime: function (time) {
    // Format in milliseconds if the total duration is short enough.
    if (this.getDuration() <= MILLIS_TIME_FORMAT_MAX_DURATION) {
      return L10N.getFormatStr("timeline.timeGraduationLabel", time.toFixed(0));
    }

    // Otherwise format in seconds.
    return L10N.getFormatStr("player.timeLabel", (time / 1000).toFixed(1));
  },

  getDuration: function () {
    return this.maxEndTime - this.minStartTime;
  },

  /**
   * Given an animation, get the various dimensions (in %) useful to draw the
   * animation in the timeline.
   */
  getAnimationDimensions: function ({state}) {
    let start = state.previousStartTime || 0;
    let duration = state.duration;
    let rate = state.playbackRate;
    let count = state.iterationCount;
    let delay = state.delay || 0;
    let endDelay = state.endDelay || 0;

    // The start position.
    let x = this.startTimeToDistance(start + (delay / rate));
    // The width for a single iteration.
    let w = this.durationToDistance(duration / rate);
    // The width for all iterations.
    let iterationW = w * (count || 1);
    // The start position of the delay.
    let delayX = delay < 0 ? x : this.startTimeToDistance(start);
    // The width of the delay.
    let delayW = this.durationToDistance(Math.abs(delay) / rate);
    // The width of the delay if it is negative, 0 otherwise.
    let negativeDelayW = delay < 0 ? delayW : 0;
    // The width of the endDelay.
    let endDelayW = this.durationToDistance(Math.abs(endDelay) / rate);
    // The start position of the endDelay.
    let endDelayX = endDelay < 0 ? x + iterationW - endDelayW
                                 : x + iterationW;

    return {x, w, iterationW, delayX, delayW, negativeDelayW,
            endDelayX, endDelayW};
  }
};

exports.TimeScale = TimeScale;

/**
 * Convert given CSS property name to JavaScript CSS name.
 * @param {String} CSS property name (e.g. background-color).
 * @return {String} JavaScript CSS property name (e.g. backgroundColor).
 */
function getJsPropertyName(cssPropertyName) {
  if (cssPropertyName == "float") {
    return "cssFloat";
  }
  // https://drafts.csswg.org/cssom/#css-property-to-idl-attribute
  return cssPropertyName.replace(/-([a-z])/gi, (str, group) => {
    return group.toUpperCase();
  });
}
exports.getJsPropertyName = getJsPropertyName;

/**
 * Turn propertyName into property-name.
 * @param {String} jsPropertyName A camelcased CSS property name. Typically
 * something that comes out of computed styles. E.g. borderBottomColor
 * @return {String} The corresponding CSS property name: border-bottom-color
 */
function getCssPropertyName(jsPropertyName) {
  return jsPropertyName.replace(/[A-Z]/g, "-$&").toLowerCase();
}
exports.getCssPropertyName = getCssPropertyName;

/**
 * Get a formatted title for this animation. This will be either:
 * "some-name", "some-name : CSS Transition", "some-name : CSS Animation",
 * "some-name : Script Animation", or "Script Animation", depending
 * if the server provides the type, what type it is and if the animation
 * has a name
 * @param {AnimationPlayerFront} animation
 */
function getFormattedAnimationTitle({state}) {
  // Older servers don't send a type, and only know about
  // CSSAnimations and CSSTransitions, so it's safe to use
  // just the name.
  if (!state.type) {
    return state.name;
  }

  // Script-generated animations may not have a name.
  if (state.type === "scriptanimation" && !state.name) {
    return L10N.getStr("timeline.scriptanimation.unnamedLabel");
  }

  return L10N.getFormatStr(`timeline.${state.type}.nameLabel`, state.name);
}
exports.getFormattedAnimationTitle = getFormattedAnimationTitle;
