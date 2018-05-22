/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const { KeyCodes } = require("devtools/client/shared/keycodes");

const { getStr } = require("../utils/l10n");
const { hasRunningAnimation } = require("../utils/utils");

class PauseResumeButton extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onKeyDown = this.onKeyDown.bind(this);

    this.state = {
      isRunning: false,
    };
  }

  componentWillMount() {
    this.updateState(this.props);
  }

  componentDidMount() {
    const targetEl = this.getKeyEventTarget();
    targetEl.addEventListener("keydown", this.onKeyDown);
  }

  componentWillReceiveProps(nextProps) {
    this.updateState(nextProps);
  }

  componentWillUnount() {
    const targetEl = this.getKeyEventTarget();
    targetEl.removeEventListener("keydown", this.onKeyDown);
  }

  getKeyEventTarget() {
    return ReactDOM.findDOMNode(this).closest("#animation-container");
  }

  onToggleAnimationsPlayState(event) {
    event.stopPropagation();
    const { setAnimationsPlayState } = this.props;
    const { isRunning } = this.state;

    setAnimationsPlayState(!isRunning);
  }

  onKeyDown(event) {
    if (event.keyCode === KeyCodes.DOM_VK_SPACE) {
      this.onToggleAnimationsPlayState(event);
    }
  }

  updateState(props) {
    const { animations } = props;
    const isRunning = hasRunningAnimation(animations);
    this.setState({ isRunning });
  }

  render() {
    const { isRunning } = this.state;

    return dom.button(
      {
        className: "pause-resume-button devtools-button" +
                   (isRunning ? "" : " paused"),
        onClick: this.onToggleAnimationsPlayState.bind(this),
        title: isRunning ?
                 getStr("timeline.resumedButtonTooltip") :
                 getStr("timeline.pausedButtonTooltip"),
      }
    );
  }
}

module.exports = PauseResumeButton;
