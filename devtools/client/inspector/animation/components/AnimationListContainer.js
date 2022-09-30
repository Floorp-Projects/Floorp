/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createRef,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const AnimationList = createFactory(
  require("resource://devtools/client/inspector/animation/components/AnimationList.js")
);
const CurrentTimeScrubber = createFactory(
  require("resource://devtools/client/inspector/animation/components/CurrentTimeScrubber.js")
);
const ProgressInspectionPanel = createFactory(
  require("resource://devtools/client/inspector/animation/components/ProgressInspectionPanel.js")
);

const {
  findOptimalTimeInterval,
} = require("resource://devtools/client/inspector/animation/utils/utils.js");
const {
  getStr,
} = require("resource://devtools/client/inspector/animation/utils/l10n.js");
const { throttle } = require("resource://devtools/shared/throttle.js");

// The minimum spacing between 2 time graduation headers in the timeline (px).
const TIME_GRADUATION_MIN_SPACING = 40;

class AnimationListContainer extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      direction: PropTypes.string.isRequired,
      dispatch: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      selectAnimation: PropTypes.func.isRequired,
      setAnimationsCurrentTime: PropTypes.func.isRequired,
      setHighlightedNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      sidebarWidth: PropTypes.number.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this._ref = createRef();

    this.updateDisplayableRange = throttle(
      this.updateDisplayableRange,
      100,
      this
    );

    this.state = {
      // tick labels and lines on the progress inspection panel
      ticks: [],
      // Displayable range.
      displayableRange: { startIndex: 0, endIndex: 0 },
    };
  }

  componentDidMount() {
    this.updateTicks(this.props);

    const current = this._ref.current;
    this._inspectionPanelEl = current.querySelector(
      ".progress-inspection-panel"
    );
    this._inspectionPanelEl.addEventListener("scroll", () => {
      this.updateDisplayableRange();
    });

    this._animationListEl = current.querySelector(".animation-list");
    const resizeObserver = new current.ownerGlobal.ResizeObserver(() => {
      this.updateDisplayableRange();
    });
    resizeObserver.observe(this._animationListEl);

    const animationItemEl = current.querySelector(".animation-item");
    this._itemHeight = animationItemEl.offsetHeight;

    this.updateDisplayableRange();
  }

  componentDidUpdate(prevProps) {
    const { timeScale, sidebarWidth } = this.props;

    if (
      timeScale.getDuration() !== prevProps.timeScale.getDuration() ||
      timeScale.zeroPositionTime !== prevProps.timeScale.zeroPositionTime ||
      sidebarWidth !== prevProps.sidebarWidth
    ) {
      this.updateTicks(this.props);
    }
  }

  /**
   * Since it takes too much time if we render all of animation graphs,
   * we restrict to render the items that are not in displaying area.
   * This function calculates the displayable item range.
   */
  updateDisplayableRange() {
    const count =
      Math.floor(this._animationListEl.offsetHeight / this._itemHeight) + 1;
    const index = Math.floor(
      this._inspectionPanelEl.scrollTop / this._itemHeight
    );
    this.setState({
      displayableRange: { startIndex: index, endIndex: index + count },
    });
  }

  updateTicks(props) {
    const { animations, timeScale } = props;
    const tickLinesEl = this._ref.current.querySelector(".tick-lines");
    const width = tickLinesEl.offsetWidth;
    const animationDuration = timeScale.getDuration();
    const minTimeInterval =
      (TIME_GRADUATION_MIN_SPACING * animationDuration) / width;
    const intervalLength = findOptimalTimeInterval(minTimeInterval);
    const intervalWidth = (intervalLength * width) / animationDuration;
    const tickCount = parseInt(width / intervalWidth, 10);
    const isAllDurationInfinity = animations.every(
      animation => animation.state.duration === Infinity
    );
    const zeroBasePosition =
      width * (timeScale.zeroPositionTime / animationDuration);
    const shiftWidth = zeroBasePosition % intervalWidth;
    const needToShift = zeroBasePosition !== 0 && shiftWidth !== 0;

    const ticks = [];
    // Need to display first graduation since position will be shifted.
    if (needToShift) {
      const label = timeScale.formatTime(timeScale.distanceToRelativeTime(0));
      ticks.push({ position: 0, label, width: shiftWidth });
    }

    for (let i = 0; i <= tickCount; i++) {
      const position = ((i * intervalWidth + shiftWidth) * 100) / width;
      const distance = timeScale.distanceToRelativeTime(position);
      const label =
        isAllDurationInfinity && i === tickCount
          ? getStr("player.infiniteTimeLabel")
          : timeScale.formatTime(distance);
      ticks.push({ position, label, width: intervalWidth });
    }

    this.setState({ ticks });
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      animations,
      direction,
      dispatch,
      getAnimatedPropertyMap,
      getNodeFromActor,
      removeAnimationsCurrentTimeListener,
      selectAnimation,
      setAnimationsCurrentTime,
      setHighlightedNode,
      setSelectedNode,
      simulateAnimation,
      timeScale,
    } = this.props;
    const { displayableRange, ticks } = this.state;

    return dom.div(
      {
        className: "animation-list-container",
        ref: this._ref,
      },
      ProgressInspectionPanel({
        indicator: CurrentTimeScrubber({
          addAnimationsCurrentTimeListener,
          direction,
          removeAnimationsCurrentTimeListener,
          setAnimationsCurrentTime,
          timeScale,
        }),
        list: AnimationList({
          animations,
          dispatch,
          displayableRange,
          getAnimatedPropertyMap,
          getNodeFromActor,
          selectAnimation,
          setHighlightedNode,
          setSelectedNode,
          simulateAnimation,
          timeScale,
        }),
        ticks,
      })
    );
  }
}

const mapStateToProps = state => {
  return {
    sidebarWidth: state.animations.sidebarSize
      ? state.animations.sidebarSize.width
      : 0,
  };
};

module.exports = connect(mapStateToProps)(AnimationListContainer);
