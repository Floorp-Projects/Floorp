/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {createNode, createSVGNode} =
  require("devtools/client/animationinspector/utils");
const {ProgressGraphHelper, appendPathElement, DEFAULT_MIN_PROGRESS_THRESHOLD} =
         require("devtools/client/animationinspector/graph-helper.js");

// Counter for linearGradient ID.
let LINEAR_GRADIENT_ID_COUNTER = 0;

/**
 * UI component responsible for displaying a list of keyframes.
 * Also, shows a graphical graph for the animation progress of one iteration.
 */
function Keyframes() {
  EventEmitter.decorate(this);
}

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

    // Calculate stroke height in viewBox to display stroke of path.
    const strokeHeightForViewBox = 0.5 / this.containerEl.clientHeight;
    // Minimum segment duration is the duration of one pixel.
    const minSegmentDuration =
      totalDuration / this.containerEl.clientWidth;

    // Set viewBox.
    graphEl.setAttribute("viewBox",
                         `0 -${ 1 + strokeHeightForViewBox }
                          ${ totalDuration }
                          ${ 1 + strokeHeightForViewBox * 2 }`);

    // Create graph helper to render the animation property graph.
    const graphHelper =
      new ProgressGraphHelper(this.containerEl.ownerDocument.defaultView,
                              propertyName, animationType, keyframes, totalDuration);

    renderPropertyGraph(graphEl, totalDuration, minSegmentDuration,
                        DEFAULT_MIN_PROGRESS_THRESHOLD, graphHelper);

    // Destroy ProgressGraphHelper resources.
    graphHelper.destroy();

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
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {ProgressGraphHelper} graphHelper - The object of ProgressGraphHalper.
 */
function renderPropertyGraph(parentEl, duration, minSegmentDuration,
                             minProgressThreshold, graphHelper) {
  const segments = graphHelper.createPathSegments(0, duration, minSegmentDuration,
                                                  minProgressThreshold);

  const graphType = graphHelper.getGraphType();
  if (graphType !== "color") {
    appendPathElement(parentEl, segments, graphType);
    return;
  }

  // Append the color to the path.
  segments.forEach(segment => {
    segment.y = 1;
  });
  const path = appendPathElement(parentEl, segments, graphType);
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
}
