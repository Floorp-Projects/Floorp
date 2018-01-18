/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

// Show max 10 iterations for infinite animations
// to give users a clue that the animation does repeat.
const MAX_INFINITE_ANIMATIONS_ITERATIONS = 10;

class TimingPath extends PureComponent {
  /**
   * Render a graph of given parameters and return as <path> element list.
   *
   * @param {Object} state
   *        State of animation.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   * @return {Array}
   *         list of <path> element.
   */
  renderGraph(state, helper) {
    // Starting time of main iteration.
    let mainIterationStartTime = 0;
    let iterationStart = state.iterationStart;
    let iterationCount = state.iterationCount ? state.iterationCount : Infinity;

    const pathList = [];

    // Append delay.
    if (state.delay > 0) {
      this.renderDelay(pathList, state, helper);
      mainIterationStartTime = state.delay;
    } else {
      const negativeDelayCount = -state.delay / state.duration;
      // Move to forward the starting point for negative delay.
      iterationStart += negativeDelayCount;
      // Consume iteration count by negative delay.
      if (iterationCount !== Infinity) {
        iterationCount -= negativeDelayCount;
      }
    }

    // Append 1st section of iterations,
    // This section is only useful in cases where iterationStart has decimals.
    // e.g.
    // if { iterationStart: 0.25, iterations: 3 }, firstSectionCount is 0.75.
    const firstSectionCount = iterationStart % 1 === 0
                            ? 0
                            : Math.min(iterationCount, 1) - iterationStart % 1;
    if (firstSectionCount) {
      this.renderFirstIteration(pathList, state,
                                mainIterationStartTime, firstSectionCount, helper);
    }

    if (iterationCount === Infinity) {
      // If the animation repeats infinitely,
      // we fill the remaining area with iteration paths.
      this.renderInfinity(pathList, state,
                          mainIterationStartTime, firstSectionCount, helper);
    } else {
      // Otherwise, we show remaining iterations, endDelay and fill.

      // Append forwards fill-mode.
      if (state.fill === "both" || state.fill === "forwards") {
        this.renderForwardsFill(pathList, state,
                                mainIterationStartTime, iterationCount, helper);
      }

      // Append middle section of iterations.
      // e.g.
      // if { iterationStart: 0.25, iterations: 3 }, middleSectionCount is 2.
      const middleSectionCount = Math.floor(iterationCount - firstSectionCount);
      this.renderMiddleIterations(pathList, state, mainIterationStartTime,
                                  firstSectionCount, middleSectionCount, helper);

      // Append last section of iterations, if there is remaining iteration.
      // e.g.
      // if { iterationStart: 0.25, iterations: 3 }, lastSectionCount is 0.25.
      const lastSectionCount = iterationCount - middleSectionCount - firstSectionCount;
      if (lastSectionCount) {
        this.renderLastIteration(pathList, state, mainIterationStartTime,
                                 firstSectionCount, middleSectionCount,
                                 lastSectionCount, helper);
      }

      // Append endDelay.
      if (state.endDelay > 0) {
        this.renderEndDelay(pathList, state,
                            mainIterationStartTime, iterationCount, helper);
      }
    }
    return pathList;
  }

  /**
   * Render 'delay' part in animation and add a <path> element to given pathList.
   *
   * @param {Array} pathList
   *        Add rendered <path> element to this array.
   * @param {Object} state
   *        State of animation.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   */
  renderDelay(pathList, state, helper) {
    const startSegment = helper.getSegment(0);
    const endSegment = { x: state.delay, y: startSegment.y };
    const segments = [startSegment, endSegment];
    pathList.push(
      dom.path(
        {
          className: "animation-delay-path",
          d: helper.toPathString(segments),
        }
      )
    );
  }

  /**
   * Render 1st section of iterations and add a <path> element to given pathList.
   * This section is only useful in cases where iterationStart has decimals.
   *
   * @param {Array} pathList
   *        Add rendered <path> element to this array.
   * @param {Object} state
   *        State of animation.
   * @param {Number} mainIterationStartTime
   *        Start time of main iteration.
   * @param {Number} firstSectionCount
   *        Iteration count of first section.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   */
  renderFirstIteration(pathList, state, mainIterationStartTime,
                       firstSectionCount, helper) {
    const startTime = mainIterationStartTime;
    const endTime = startTime + firstSectionCount * state.duration;
    const segments = helper.createPathSegments(startTime, endTime);
    pathList.push(
      dom.path(
        {
          className: "animation-iteration-path",
          d: helper.toPathString(segments),
        }
      )
    );
  }

  /**
   * Render middle iterations and add <path> elements to given pathList.
   *
   * @param {Array} pathList
   *        Add rendered <path> elements to this array.
   * @param {Object} state
   *        State of animation.
   * @param {Number} mainIterationStartTime
   *        Starting time of main iteration.
   * @param {Number} firstSectionCount
   *        Iteration count of first section.
   * @param {Number} middleSectionCount
   *        Iteration count of middle section.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   */
  renderMiddleIterations(pathList, state, mainIterationStartTime,
                         firstSectionCount, middleSectionCount, helper) {
    const offset = mainIterationStartTime + firstSectionCount * state.duration;
    for (let i = 0; i < middleSectionCount; i++) {
      // Get the path segments of each iteration.
      const startTime = offset + i * state.duration;
      const endTime = startTime + state.duration;
      const segments = helper.createPathSegments(startTime, endTime);
      pathList.push(
        dom.path(
          {
            className: "animation-iteration-path",
            d: helper.toPathString(segments),
          }
        )
      );
    }
  }

  /**
   * Render last section of iterations and add a <path> element to given pathList.
   * This section is only useful in cases where iterationStart has decimals.
   *
   * @param {Array} pathList
   *        Add rendered <path> elements to this array.
   * @param {Object} state
   *        State of animation.
   * @param {Number} mainIterationStartTime
   *        Starting time of main iteration.
   * @param {Number} firstSectionCount
   *        Iteration count of first section.
   * @param {Number} middleSectionCount
   *        Iteration count of middle section.
   * @param {Number} lastSectionCount
   *        Iteration count of last section.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   */
  renderLastIteration(pathList, state, mainIterationStartTime, firstSectionCount,
                      middleSectionCount, lastSectionCount, helper) {
    const startTime = mainIterationStartTime
                    + (firstSectionCount + middleSectionCount) * state.duration;
    const endTime = startTime + lastSectionCount * state.duration;
    const segments = helper.createPathSegments(startTime, endTime);
    pathList.push(
      dom.path(
        {
          className: "animation-iteration-path",
          d: helper.toPathString(segments),
        }
      )
    );
  }

  /**
   * Render infinity iterations and add <path> elements to given pathList.
   *
   * @param {Array} pathList
   *        Add rendered <path> elements to this array.
   * @param {Object} state
   *        State of animation.
   * @param {Number} mainIterationStartTime
   *        Starting time of main iteration.
   * @param {Number} firstSectionCount
   *        Iteration count of first section.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   */
  renderInfinity(pathList, state, mainIterationStartTime, firstSectionCount, helper) {
    // Calculate the number of iterations to display,
    // with a maximum of MAX_INFINITE_ANIMATIONS_ITERATIONS
    let uncappedInfinityIterationCount =
      (helper.totalDuration - firstSectionCount * state.duration) / state.duration;
    // If there is a small floating point error resulting in, e.g. 1.0000001
    // ceil will give us 2 so round first.
    uncappedInfinityIterationCount =
      parseFloat(uncappedInfinityIterationCount.toPrecision(6));
    const infinityIterationCount = Math.min(MAX_INFINITE_ANIMATIONS_ITERATIONS,
                                            Math.ceil(uncappedInfinityIterationCount));

    // Append first full iteration path.
    const firstStartTime =
      mainIterationStartTime + firstSectionCount * state.duration;
    const firstEndTime = firstStartTime + state.duration;
    const firstSegments = helper.createPathSegments(firstStartTime, firstEndTime);
    pathList.push(
      dom.path(
        {
          className: "animation-iteration-path",
          d: helper.toPathString(firstSegments),
        }
      )
    );

    // Append other iterations. We can copy first segments.
    const isAlternate = state.direction.match(/alternate/);
    for (let i = 1; i < infinityIterationCount; i++) {
      const startTime = firstStartTime + i * state.duration;
      let segments;
      if (isAlternate && i % 2) {
        // Copy as reverse.
        segments = firstSegments.map(segment => {
          return { x: firstEndTime - segment.x + startTime, y: segment.y };
        });
      } else {
        // Copy as is.
        segments = firstSegments.map(segment => {
          return { x: segment.x - firstStartTime + startTime, y: segment.y };
        });
      }
      pathList.push(
        dom.path(
          {
            className: "animation-iteration-path infinity",
            d: helper.toPathString(segments),
          }
        )
      );
    }
  }

  /**
   * Render 'endDelay' part in animation and add a <path> element to given pathList.
   *
   * @param {Array} pathList
   *        Add rendered <path> element to this array.
   * @param {Object} state
   *        State of animation.
   * @param {Number} mainIterationStartTime
   *        Starting time of main iteration.
   * @param {Number} iterationCount
   *        Iteration count of whole animation.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   */
  renderEndDelay(pathList, state, mainIterationStartTime, iterationCount, helper) {
    const startTime = mainIterationStartTime + iterationCount * state.duration;
    const startSegment = helper.getSegment(startTime);
    const endSegment = { x: startTime + state.endDelay, y: startSegment.y };
    pathList.push(
      dom.path(
        {
          className: "animation-enddelay-path",
          d: helper.toPathString([startSegment, endSegment]),
        }
      )
    );
  }

  /**
   * Render 'fill' for forwards part in animation and
   * add a <path> element to given pathList.
   *
   * @param {Array} pathList
   *        Add rendered <path> element to this array.
   * @param {Object} state
   *        State of animation.
   * @param {Number} mainIterationStartTime
   *        Starting time of main iteration.
   * @param {Number} iterationCount
   *        Iteration count of whole animation.
   * @param {SummaryGraphHelper} helper
   *        Instance of SummaryGraphHelper.
   */
  renderForwardsFill(pathList, state, mainIterationStartTime, iterationCount, helper) {
    const startTime = mainIterationStartTime + iterationCount * state.duration
                    + (state.endDelay > 0 ? state.endDelay : 0);
    const startSegment = helper.getSegment(startTime);
    const endSegment = { x: helper.totalDuration, y: startSegment.y };
    pathList.push(
      dom.path(
        {
          className: "animation-fill-forwards-path",
          d: helper.toPathString([startSegment, endSegment]),
        }
      )
    );
  }
}

module.exports = TimingPath;
