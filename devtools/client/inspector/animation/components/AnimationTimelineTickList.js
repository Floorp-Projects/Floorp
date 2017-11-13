/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, DOM: dom, PropTypes, PureComponent } =
  require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const AnimationTimelineTickItem = createFactory(require("./AnimationTimelineTickItem"));

const TimeScale = require("../utils/timescale");
const { findOptimalTimeInterval } = require("../utils/utils");

// The minimum spacing between 2 time graduation headers in the timeline (px).
const TIME_GRADUATION_MIN_SPACING = 40;

class AnimationTimelineTickList extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      tickList: [],
    };
  }

  componentDidMount() {
    this.updateTickList();
  }

  updateTickList() {
    const { animations } = this.props;
    const timeScale = new TimeScale(animations);
    const tickListEl = ReactDOM.findDOMNode(this);
    const width = tickListEl.offsetWidth;
    const animationDuration = timeScale.getDuration();
    const minTimeInterval = TIME_GRADUATION_MIN_SPACING * animationDuration / width;
    const intervalLength = findOptimalTimeInterval(minTimeInterval);
    const intervalWidth = intervalLength * width / animationDuration;
    const tickCount = width / intervalWidth;
    const intervalPositionPercentage = 100 * intervalWidth / width;

    const tickList = [];
    for (let i = 0; i <= tickCount; i++) {
      const position = i * intervalPositionPercentage;
      const timeTickLabel =
        timeScale.formatTime(timeScale.distanceToRelativeTime(position));
      tickList.push({ position, timeTickLabel });
    }

    this.setState({ tickList });
  }

  render() {
    const { tickList } = this.state;

    return dom.div(
      {
        className: "animation-timeline-tick-list"
      },
      tickList.map(tickItem => AnimationTimelineTickItem(tickItem))
    );
  }
}

module.exports = AnimationTimelineTickList;
