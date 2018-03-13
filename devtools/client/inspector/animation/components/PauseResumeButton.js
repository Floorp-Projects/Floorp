/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr } = require("../utils/l10n");

class PauseResumeButton extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isPlaying: false,
    };
  }

  componentWillMount() {
    this.updateState(this.props);
  }

  componentWillReceiveProps(nextProps) {
    this.updateState(nextProps);
  }

  onClick() {
    const { setAnimationsPlayState } = this.props;
    const { isPlaying } = this.state;

    setAnimationsPlayState(!isPlaying);
  }

  updateState() {
    const { animations } = this.props;
    const isPlaying = animations.some(({state}) => state.playState === "running");
    this.setState({ isPlaying });
  }

  render() {
    const { isPlaying } = this.state;

    return dom.button(
      {
        className: "pause-resume-button devtools-button" +
                   (isPlaying ? "" : " paused"),
        onClick: this.onClick.bind(this),
        title: isPlaying ?
                 getStr("timeline.resumedButtonTooltip") :
                 getStr("timeline.pausedButtonTooltip"),
      }
    );
  }
}

module.exports = PauseResumeButton;
