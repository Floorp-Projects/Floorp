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

const CurrentTimeLabel = createFactory(
  require("resource://devtools/client/inspector/animation/components/CurrentTimeLabel.js")
);
const PauseResumeButton = createFactory(
  require("resource://devtools/client/inspector/animation/components/PauseResumeButton.js")
);
const PlaybackRateSelector = createFactory(
  require("resource://devtools/client/inspector/animation/components/PlaybackRateSelector.js")
);
const RewindButton = createFactory(
  require("resource://devtools/client/inspector/animation/components/RewindButton.js")
);

class AnimationToolbar extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      rewindAnimationsCurrentTime: PropTypes.func.isRequired,
      setAnimationsPlaybackRate: PropTypes.func.isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      animations,
      removeAnimationsCurrentTimeListener,
      rewindAnimationsCurrentTime,
      setAnimationsPlaybackRate,
      setAnimationsPlayState,
      timeScale,
    } = this.props;

    return dom.div(
      {
        className: "animation-toolbar devtools-toolbar",
      },
      RewindButton({
        rewindAnimationsCurrentTime,
      }),
      PauseResumeButton({
        animations,
        setAnimationsPlayState,
      }),
      PlaybackRateSelector({
        animations,
        setAnimationsPlaybackRate,
      }),
      CurrentTimeLabel({
        addAnimationsCurrentTimeListener,
        removeAnimationsCurrentTimeListener,
        timeScale,
      })
    );
  }
}

module.exports = AnimationToolbar;
