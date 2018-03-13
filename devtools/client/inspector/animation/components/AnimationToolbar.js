/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const CurrentTimeLabel = createFactory(require("./CurrentTimeLabel"));
const PauseResumeButton = createFactory(require("./PauseResumeButton"));
const RewindButton = createFactory(require("./RewindButton"));

class AnimationToolbar extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      rewindAnimationsCurrentTime: PropTypes.func.isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      animations,
      removeAnimationsCurrentTimeListener,
      rewindAnimationsCurrentTime,
      setAnimationsPlayState,
    } = this.props;

    return dom.div(
      {
        className: "animation-toolbar devtools-toolbar",
      },
      RewindButton(
        {
          rewindAnimationsCurrentTime,
        }
      ),
      PauseResumeButton(
        {
          animations,
          setAnimationsPlayState,
        }
      ),
      CurrentTimeLabel(
        {
          addAnimationsCurrentTimeListener,
          removeAnimationsCurrentTimeListener,
        }
      )
    );
  }
}

module.exports = AnimationToolbar;
