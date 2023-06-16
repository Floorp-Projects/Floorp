/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const AnimationName = createFactory(
  require("resource://devtools/client/inspector/animation/components/graph/AnimationName.js")
);
const DelaySign = createFactory(
  require("resource://devtools/client/inspector/animation/components/graph/DelaySign.js")
);
const EndDelaySign = createFactory(
  require("resource://devtools/client/inspector/animation/components/graph/EndDelaySign.js")
);
const SummaryGraphPath = createFactory(
  require("resource://devtools/client/inspector/animation/components/graph/SummaryGraphPath.js")
);

const {
  getFormattedTitle,
  getFormatStr,
  getStr,
  numberWithDecimals,
} = require("resource://devtools/client/inspector/animation/utils/l10n.js");

class SummaryGraph extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      selectAnimation: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onClick = this.onClick.bind(this);
  }

  onClick(event) {
    event.stopPropagation();
    this.props.selectAnimation(this.props.animation);
  }

  getTitleText(state) {
    const getTime = time =>
      getFormatStr("player.timeLabel", numberWithDecimals(time / 1000, 2));
    const getTimeOrInfinity = time =>
      time === Infinity ? getStr("player.infiniteDurationText") : getTime(time);

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
    text += getTimeOrInfinity(state.duration);
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
      text +=
        state.iterationCount || getStr("player.infiniteIterationCountText");
      text += "\n";
    }

    // Adding the iteration start.
    if (state.iterationStart !== 0) {
      text += getFormatStr(
        "player.animationIterationStartLabel2",
        state.iterationStart,
        getTimeOrInfinity(state.iterationStart * state.duration)
      );
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
    if (
      state.animationTimingFunction &&
      state.animationTimingFunction !== "ease"
    ) {
      text += getStr("player.animationTimingFunctionLabel") + " ";
      text += state.animationTimingFunction;
      text += "\n";
    }

    // Adding a note that the animation is running on the compositor thread if
    // needed.
    if (state.propertyState) {
      if (
        state.propertyState.every(propState => propState.runningOnCompositor)
      ) {
        text += getStr("player.allPropertiesOnCompositorTooltip");
      } else if (
        state.propertyState.some(propState => propState.runningOnCompositor)
      ) {
        text += getStr("player.somePropertiesOnCompositorTooltip");
      }
    } else if (state.isRunningOnCompositor) {
      text += getStr("player.runningOnCompositorTooltip");
    }

    return text;
  }

  render() {
    const { animation, getAnimatedPropertyMap, simulateAnimation, timeScale } =
      this.props;

    const { iterationCount } = animation.state;
    const { delay, endDelay } = animation.state.absoluteValues;

    return dom.div(
      {
        className:
          "animation-summary-graph" +
          (animation.state.isRunningOnCompositor ? " compositor" : ""),
        onClick: this.onClick,
        title: this.getTitleText(animation.state),
      },
      SummaryGraphPath({
        animation,
        getAnimatedPropertyMap,
        simulateAnimation,
        timeScale,
      }),
      delay
        ? DelaySign({
            animation,
            timeScale,
          })
        : null,
      iterationCount && endDelay
        ? EndDelaySign({
            animation,
            timeScale,
          })
        : null,
      animation.state.name
        ? AnimationName({
            animation,
          })
        : null
    );
  }
}

module.exports = SummaryGraph;
