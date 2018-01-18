/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimationName = createFactory(require("./AnimationName"));
const DelaySign = createFactory(require("./DelaySign"));
const EndDelaySign = createFactory(require("./EndDelaySign"));
const SummaryGraphPath = createFactory(require("./SummaryGraphPath"));

const { getFormatStr, getStr, numberWithDecimals } = require("../../utils/l10n");

class SummaryGraph extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  getTitleText(state) {
    const getTime =
      time => getFormatStr("player.timeLabel", numberWithDecimals(time / 1000, 2));

    let text = "";

    // Adding the name.
    text += getFormattedTitle(state);
    text += "\n";

    // Adding the delay.
    if (state.delay) {
      text += getStr("player.animationDelayLabel") + " ";
      text += getTime(state.delay);
      text += "\n";
    }

    // Adding the duration.
    text += getStr("player.animationDurationLabel") + " ";
    text += getTime(state.duration);
    text += "\n";

    // Adding the endDelay.
    if (state.endDelay) {
      text += getStr("player.animationEndDelayLabel") + " ";
      text += getTime(state.endDelay);
      text += "\n";
    }

    // Adding the iteration count (the infinite symbol, or an integer).
    if (state.iterationCount !== 1) {
      text += getStr("player.animationIterationCountLabel") + " ";
      text += state.iterationCount || getStr("player.infiniteIterationCountText");
      text += "\n";
    }

    // Adding the iteration start.
    if (state.iterationStart !== 0) {
      const iterationStartTime = state.iterationStart * state.duration / 1000;
      text += getFormatStr("player.animationIterationStartLabel",
                           state.iterationStart,
                           numberWithDecimals(iterationStartTime, 2));
      text += "\n";
    }

    // Adding the easing if it is not "linear".
    if (state.easing && state.easing !== "linear") {
      text += getStr("player.animationOverallEasingLabel") + " ";
      text += state.easing;
      text += "\n";
    }

    // Adding the fill mode.
    if (state.fill && state.fill !== "none") {
      text += getStr("player.animationFillLabel") + " ";
      text += state.fill;
      text += "\n";
    }

    // Adding the direction mode if it is not "normal".
    if (state.direction && state.direction !== "normal") {
      text += getStr("player.animationDirectionLabel") + " ";
      text += state.direction;
      text += "\n";
    }

    // Adding the playback rate if it's different than 1.
    if (state.playbackRate !== 1) {
      text += getStr("player.animationRateLabel") + " ";
      text += state.playbackRate;
      text += "\n";
    }

    // Adding the animation-timing-function
    // if it is not "ease" which is default value for CSS Animations.
    if (state.animationTimingFunction && state.animationTimingFunction !== "ease") {
      text += getStr("player.animationTimingFunctionLabel") + " ";
      text += state.animationTimingFunction;
      text += "\n";
    }

    // Adding a note that the animation is running on the compositor thread if
    // needed.
    if (state.propertyState) {
      if (state.propertyState.every(propState => propState.runningOnCompositor)) {
        text += getStr("player.allPropertiesOnCompositorTooltip");
      } else if (state.propertyState.some(propState => propState.runningOnCompositor)) {
        text += getStr("player.somePropertiesOnCompositorTooltip");
      }
    } else if (state.isRunningOnCompositor) {
      text += getStr("player.runningOnCompositorTooltip");
    }

    return text;
  }

  render() {
    const {
      animation,
      getAnimatedPropertyMap,
      simulateAnimation,
      timeScale,
    } = this.props;

    return dom.div(
      {
        className: "animation-summary-graph" +
                   (animation.state.isRunningOnCompositor ? " compositor" : ""),
        title: this.getTitleText(animation.state),
      },
      SummaryGraphPath(
        {
          animation,
          getAnimatedPropertyMap,
          simulateAnimation,
          timeScale,
        }
      ),
      animation.state.delay ?
        DelaySign(
          {
            animation,
            timeScale,
          }
        )
      :
      null,
      animation.state.iterationCount && animation.state.endDelay ?
        EndDelaySign(
          {
            animation,
            timeScale,
          }
        )
      :
      null,
      animation.state.name ?
        AnimationName(
          {
            animation
          }
        )
      :
      null
    );
  }
}

/**
 * Get a formatted title for this animation. This will be either:
 * "%S", "%S : CSS Transition", "%S : CSS Animation",
 * "%S : Script Animation", or "Script Animation", depending
 * if the server provides the type, what type it is and if the animation
 * has a name.
 *
 * @param {Object} state
 */
function getFormattedTitle(state) {
  // Older servers don't send a type, and only know about
  // CSSAnimations and CSSTransitions, so it's safe to use
  // just the name.
  if (!state.type) {
    return state.name;
  }

  // Script-generated animations may not have a name.
  if (state.type === "scriptanimation" && !state.name) {
    return getStr("timeline.scriptanimation.unnamedLabel");
  }

  return getFormatStr(`timeline.${state.type}.nameLabel`, state.name);
}

module.exports = SummaryGraph;
