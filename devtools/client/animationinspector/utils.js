/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu} = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
var {loader} = Cu.import("resource://devtools/shared/Loader.jsm");
loader.lazyRequireGetter(this, "EventEmitter",
                               "devtools/shared/event-emitter");

const STRINGS_URI = "chrome://devtools/locale/animationinspector.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);
// How many times, maximum, can we loop before we find the optimal time
// interval in the timeline graph.
const OPTIMAL_TIME_INTERVAL_MAX_ITERS = 100;
// Background time graduations should be multiple of this number of millis.
const TIME_INTERVAL_MULTIPLE = 25;
const TIME_INTERVAL_SCALES = 3;
// The default minimum spacing between time graduations in px.
const TIME_GRADUATION_MIN_SPACING = 10;
// RGB color for the time interval background.
const TIME_INTERVAL_COLOR = [128, 136, 144];
// byte
const TIME_INTERVAL_OPACITY_MIN = 32;
// byte
const TIME_INTERVAL_OPACITY_ADD = 32;

const MILLIS_TIME_FORMAT_MAX_DURATION = 4000;

/**
 * DOM node creation helper function.
 * @param {Object} Options to customize the node to be created.
 * - nodeType {String} Optional, defaults to "div",
 * - attributes {Object} Optional attributes object like
 *   {attrName1:value1, attrName2: value2, ...}
 * - parent {DOMNode} Mandatory node to append the newly created node to.
 * - textContent {String} Optional text for the node.
 * @return {DOMNode} The newly created node.
 */
function createNode(options) {
  if (!options.parent) {
    throw new Error("Missing parent DOMNode to create new node");
  }

  let type = options.nodeType || "div";
  let node = options.parent.ownerDocument.createElement(type);

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
 * Given a data-scale, draw the background for a graph (vertical lines) into a
 * canvas and set that canvas as an image-element with an ID that can be used
 * from CSS.
 * @param {Document} document The document where the image-element should be set.
 * @param {String} id The ID for the image-element.
 * @param {Number} graphWidth The width of the graph.
 * @param {Number} timeScale How many px is 1ms in the graph.
 */
function drawGraphElementBackground(document, id, graphWidth, timeScale) {
  let canvas = document.createElement("canvas");
  let ctx = canvas.getContext("2d");

  // Set the canvas width (as requested) and height (1px, repeated along the Y
  // axis).
  canvas.width = graphWidth;
  canvas.height = 1;

  // Create the image data array which will receive the pixels.
  let imageData = ctx.createImageData(canvas.width, canvas.height);
  let pixelArray = imageData.data;

  let buf = new ArrayBuffer(pixelArray.length);
  let view8bit = new Uint8ClampedArray(buf);
  let view32bit = new Uint32Array(buf);

  // Build new millisecond tick lines...
  let [r, g, b] = TIME_INTERVAL_COLOR;
  let alphaComponent = TIME_INTERVAL_OPACITY_MIN;
  let interval = findOptimalTimeInterval(timeScale);

  // Insert one pixel for each division on each scale.
  for (let i = 1; i <= TIME_INTERVAL_SCALES; i++) {
    let increment = interval * Math.pow(2, i);
    for (let x = 0; x < canvas.width; x += increment) {
      let position = x | 0;
      view32bit[position] = (alphaComponent << 24) | (b << 16) | (g << 8) | r;
    }
    alphaComponent += TIME_INTERVAL_OPACITY_ADD;
  }

  // Flush the image data and cache the waterfall background.
  pixelArray.set(view8bit);
  ctx.putImageData(imageData, 0, 0);
  document.mozSetImageElement(id, canvas);
}

exports.drawGraphElementBackground = drawGraphElementBackground;

/**
 * Find the optimal interval between time graduations in the animation timeline
 * graph based on a time scale and a minimum spacing.
 * @param {Number} timeScale How many px is 1ms in the graph.
 * @param {Number} minSpacing The minimum spacing between 2 graduations,
 * defaults to TIME_GRADUATION_MIN_SPACING.
 * @return {Number} The optimal interval, in pixels.
 */
function findOptimalTimeInterval(timeScale,
                                 minSpacing=TIME_GRADUATION_MIN_SPACING) {
  let timingStep = TIME_INTERVAL_MULTIPLE;
  let numIters = 0;

  if (timeScale > minSpacing) {
    return timeScale;
  }

  while (true) {
    let scaledStep = timeScale * timingStep;
    if (++numIters > OPTIMAL_TIME_INTERVAL_MAX_ITERS) {
      return scaledStep;
    }
    if (scaledStep < minSpacing) {
      timingStep *= 2;
      continue;
    }
    return scaledStep;
  }
}

exports.findOptimalTimeInterval = findOptimalTimeInterval;

/**
 * The TargetNodeHighlighter util is a helper for AnimationTargetNode components
 * that is used to lock the highlighter on animated nodes in the page.
 * It instantiates a new highlighter that is then shared amongst all instances
 * of AnimationTargetNode. This is useful because that means showing the
 * highlighter on one animated node will unhighlight the previously highlighted
 * one, but will not interfere with the default inspector highlighter.
 */
var TargetNodeHighlighter = {
  highlighter: null,
  isShown: false,

  highlight: Task.async(function*(animationTargetNode) {
    if (!this.highlighter) {
      let hUtils = animationTargetNode.inspector.toolbox.highlighterUtils;
      this.highlighter = yield hUtils.getHighlighterByType("BoxModelHighlighter");
    }

    yield this.highlighter.show(animationTargetNode.nodeFront);
    this.isShown = true;
    this.emit("highlighted", animationTargetNode);
  }),

  unhighlight: Task.async(function*() {
    if (!this.highlighter || !this.isShown) {
      return;
    }

    yield this.highlighter.hide();
    this.isShown = false;
    this.emit("unhighlighted");
  })
};

EventEmitter.decorate(TargetNodeHighlighter);
exports.TargetNodeHighlighter = TargetNodeHighlighter;

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
      return new Array((max+"").length - (nb+"").length + 1).join("0") + nb;
    }
    return nb;
  }

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
  addAnimation: function(state) {
    let {previousStartTime, delay, duration,
         iterationCount, playbackRate} = state;

    // Negative-delayed animations have their startTimes set such that we would
    // be displaying the delay outside the time window if we didn't take it into
    // account here.
    let relevantDelay = delay < 0 ? delay / playbackRate : 0;
    previousStartTime = previousStartTime || 0;

    this.minStartTime = Math.min(this.minStartTime,
                                 previousStartTime + relevantDelay);
    let length = (delay / playbackRate) +
                 ((duration / playbackRate) *
                  (!iterationCount ? 1 : iterationCount));
    let endTime = previousStartTime + length;
    this.maxEndTime = Math.max(this.maxEndTime, endTime);
  },

  /**
   * Reset the current time scale.
   */
  reset: function() {
    this.minStartTime = Infinity;
    this.maxEndTime = 0;
  },

  /**
   * Convert a startTime to a distance in %, in the current time scale.
   * @param {Number} time
   * @return {Number}
   */
  startTimeToDistance: function(time) {
    time -= this.minStartTime;
    return this.durationToDistance(time);
  },

  /**
   * Convert a duration to a distance in %, in the current time scale.
   * @param {Number} time
   * @return {Number}
   */
  durationToDistance: function(duration) {
    return duration * 100 / this.getDuration();
  },

  /**
   * Convert a distance in % to a time, in the current time scale.
   * @param {Number} distance
   * @return {Number}
   */
  distanceToTime: function(distance) {
    return this.minStartTime + (this.getDuration() * distance / 100);
  },

  /**
   * Convert a distance in % to a time, in the current time scale.
   * The time will be relative to the current minimum start time.
   * @param {Number} distance
   * @return {Number}
   */
  distanceToRelativeTime: function(distance) {
    let time = this.distanceToTime(distance);
    return time - this.minStartTime;
  },

  /**
   * Depending on the time scale, format the given time as milliseconds or
   * seconds.
   * @param {Number} time
   * @return {String} The formatted time string.
   */
  formatTime: function(time) {
    // Format in milliseconds if the total duration is short enough.
    if (this.getDuration() <= MILLIS_TIME_FORMAT_MAX_DURATION) {
      return L10N.getFormatStr("timeline.timeGraduationLabel", time.toFixed(0));
    }

    // Otherwise format in seconds.
    return L10N.getFormatStr("player.timeLabel", (time / 1000).toFixed(1));
  },

  getDuration: function() {
    return this.maxEndTime - this.minStartTime;
  },

  /**
   * Given an animation, get the various dimensions (in %) useful to draw the
   * animation in the timeline.
   */
  getAnimationDimensions: function({state}) {
    let start = state.previousStartTime || 0;
    let duration = state.duration;
    let rate = state.playbackRate;
    let count = state.iterationCount;
    let delay = state.delay || 0;

    // The start position.
    let x = this.startTimeToDistance(start + (delay / rate));
    // The width for a single iteration.
    let w = this.durationToDistance(duration / rate);
    // The width for all iterations.
    let iterationW = w * (count || 1);
    // The start position of the delay.
    let delayX = this.durationToDistance((delay < 0 ? 0 : delay) / rate);
    // The width of the delay.
    let delayW = this.durationToDistance(Math.abs(delay) / rate);
    // The width of the delay if it is positive, 0 otherwise.
    let negativeDelayW = delay < 0 ? delayW : 0;

    return {x, w, iterationW, delayX, delayW, negativeDelayW};
  }
};

exports.TimeScale = TimeScale;
