/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } =
  require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const AnimationTimelineTickList = createFactory(require("./AnimationTimelineTickList"));
const CurrentTimeScrubberController =
  createFactory(require("./CurrentTimeScrubberController"));

class AnimationListHeader extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      setAnimationsCurrentTime: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      removeAnimationsCurrentTimeListener,
      setAnimationsCurrentTime,
      timeScale,
    } = this.props;

    return dom.div(
      {
        className: "animation-list-header devtools-toolbar"
      },
      AnimationTimelineTickList(
        {
          timeScale
        }
      ),
      CurrentTimeScrubberController(
        {
          addAnimationsCurrentTimeListener,
          removeAnimationsCurrentTimeListener,
          setAnimationsCurrentTime,
          timeScale,
        }
      )
    );
  }
}

module.exports = AnimationListHeader;
