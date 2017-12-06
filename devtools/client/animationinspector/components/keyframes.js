/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {createNode, createSVGNode} =
  require("devtools/client/animationinspector/utils");
const { ProgressGraphHelper, } =
  require("devtools/client/animationinspector/graph-helper.js");

// Counter for linearGradient ID.
let LINEAR_GRADIENT_ID_COUNTER = 0;

/**
 * UI component responsible for displaying a list of keyframes.
 * Also, shows a graphical graph for the animation progress of one iteration.
 */
function Keyframes() {}

exports.Keyframes = Keyframes;

Keyframes.prototype = {
  init: function (containerEl) {
    this.containerEl = containerEl;

    this.keyframesEl = createNode({
      parent: this.containerEl,
      attributes: {"class": "keyframes"}
    });
  },

  destroy: function () {
    this.keyframesEl.remove();
    this.containerEl = this.keyframesEl = this.animation = null;
  },

  render: function ({keyframes, propertyName, animation, animationType}) {
    this.keyframes = keyframes;
    this.propertyName = propertyName;
    this.animation = animation;

    // Create graph element.
    const graphEl = createSVGNode({
      parent: this.keyframesEl,
      nodeType: "svg",
      attributes: {
        "preserveAspectRatio": "none"
      }
    });

    // This visual is only one iteration,
    // so we use animation.state.duration as total duration.
    const totalDuration = animation.state.duration;

    // Minimum segment duration is the duration of one pixel.
    const minSegmentDuration =
      totalDuration / this.containerEl.clientWidth;

    // Create graph helper to render the animation property graph.
    const win = this.containerEl.ownerGlobal;
    const graphHelper =
      new ProgressGraphHelper(win, propertyName, animationType, keyframes, totalDuration);

    renderPropertyGraph(graphEl, totalDuration, minSegmentDuration, graphHelper);

    // Destroy ProgressGraphHelper resources.
    graphHelper.destroy();

    // Set viewBox which includes invisible stroke width.
    // At first, calculate invisible stroke width from maximum width.
    // The reason why divide by 2 is that half of stroke width will be invisible
    // if we use 0 or 1 for y axis.
    const maxStrokeWidth =
      win.getComputedStyle(graphEl.querySelector(".keyframes svg .hint")).strokeWidth;
    const invisibleStrokeWidthInViewBox =
      maxStrokeWidth / 2 / this.containerEl.clientHeight;
    graphEl.setAttribute("viewBox",
                         `0 -${ 1 + invisibleStrokeWidthInViewBox }
                          ${ totalDuration }
                          ${ 1 + invisibleStrokeWidthInViewBox * 2 }`);

    // Append elements to display keyframe values.
    this.keyframesEl.classList.add(animation.state.type);
    for (let frame of this.keyframes) {
      createNode({
        parent: this.keyframesEl,
        attributes: {
          "class": "frame",
          "style": `left:${frame.offset * 100}%;`,
          "data-offset": frame.offset,
          "data-property": propertyName,
          "title": frame.value
        }
      });
    }
  }
};

/**
 * Render a graph representing the progress of the animation over one iteration.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Number} duration - Duration of one iteration.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {ProgressGraphHelper} graphHelper - The object of ProgressGraphHalper.
 */
function renderPropertyGraph(parentEl, duration, minSegmentDuration, graphHelper) {
  const segments = graphHelper.createPathSegments(duration, minSegmentDuration);

  const graphType = graphHelper.getGraphType();
  if (graphType !== "color") {
    graphHelper.appendShapePath(parentEl, segments, graphType);
    renderEasingHint(parentEl, segments, graphHelper);
    return;
  }

  // Append the color to the path.
  segments.forEach(segment => {
    segment.y = 1;
  });
  const path = graphHelper.appendShapePath(parentEl, segments, graphType);
  const defEl = createSVGNode({
    parent: parentEl,
    nodeType: "def"
  });
  const id = `color-property-${ LINEAR_GRADIENT_ID_COUNTER++ }`;
  const linearGradientEl = createSVGNode({
    parent: defEl,
    nodeType: "linearGradient",
    attributes: {
      "id": id
    }
  });
  segments.forEach(segment => {
    createSVGNode({
      parent: linearGradientEl,
      nodeType: "stop",
      attributes: {
        "stop-color": segment.style,
        "offset": segment.x / duration
      }
    });
  });
  path.style.fill = `url(#${ id })`;

  renderEasingHintForColor(parentEl, graphHelper);
}

/**
 * Renders the easing hint.
 * This method renders an emphasized path over the easing path for a keyframe.
 * It appears when hovering over the easing.
 * It also renders a tooltip that appears when hovering.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Array} path segments - [{x: {Number} time, y: {Number} progress}, ...]
 * @param {ProgressGraphHelper} graphHelper - The object of ProgressGraphHelper.
 */
function renderEasingHint(parentEl, segments, helper) {
  const keyframes = helper.getKeyframes();
  const duration = helper.getDuration();

  // Split segments for each keyframe.
  for (let i = 0, indexOfSegments = 0; i < keyframes.length - 1; i++) {
    const startKeyframe = keyframes[i];
    const endKeyframe = keyframes[i + 1];
    const endTime = endKeyframe.offset * duration;

    const keyframeSegments = [];
    for (; indexOfSegments < segments.length; indexOfSegments++) {
      const segment = segments[indexOfSegments];
      keyframeSegments.push(segment);

      if (segment.x === endTime) {
        break;
      }
    }

    // Append easing hint as text and emphasis path.
    const gEl = createSVGNode({
      parent: parentEl,
      nodeType: "g"
    });
    createSVGNode({
      parent: gEl,
      nodeType: "title",
      textContent: startKeyframe.easing
    });
    helper.appendLinePath(gEl, keyframeSegments, `${helper.getGraphType()} hint`);
  }
}

/**
 * Render easing hint for properties that are represented by color.
 * This method render as text only.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {ProgressGraphHelper} graphHelper - The object of ProgressGraphHalper.
 */
function renderEasingHintForColor(parentEl, helper) {
  const keyframes = helper.getKeyframes();
  const duration = helper.getDuration();

  // Split segments for each keyframe.
  for (let i = 0; i < keyframes.length - 1; i++) {
    const startKeyframe = keyframes[i];
    const startTime = startKeyframe.offset * duration;
    const endKeyframe = keyframes[i + 1];
    const endTime = endKeyframe.offset * duration;

    // Append easing hint.
    const gEl = createSVGNode({
      parent: parentEl,
      nodeType: "g"
    });
    createSVGNode({
      parent: gEl,
      nodeType: "title",
      textContent: startKeyframe.easing
    });
    createSVGNode({
      parent: gEl,
      nodeType: "rect",
      attributes: {
        x: startTime,
        y: -1,
        width: endTime - startTime,
        height: 1,
        class: "hint",
      }
    });
  }
}
