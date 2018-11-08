/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div } = dom;

class WebReplayPlayer extends Component {
  static get propTypes() {
    return {
      threadClient: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.state = { progress: 0 };
  }

  componentDidMount() {
    this.props.threadClient.addListener("paused", this.dispatchPaused.bind(this));
    this.props.threadClient.addListener("resumed", this.dispatchResumed.bind(this));
  }

  dispatchPaused(_, packet) {
    if (packet && packet.recordingProgress) {
      this.setState({ progress: packet.recordingProgress });
    }
  }

  dispatchResumed(_, packet) {
    this.setState({ progress: 0 });
  }

  renderCommands() {
    const { progress } = this.state;
    const { threadClient } = this.props;
    const paused = progress !== 0;

    if (paused) {
      return [
        div(
          { className: "command-button" },
          div({ className: "rewind-button btn", onClick: () => threadClient.rewind() })
        ),
        div(
          { className: "command-button" },
          div({ className: "play-button btn", onClick: () => threadClient.resume() })
        ),
      ];
    }

    return [
      div(
        { className: "command-button" },
        div({ className: "pause-button btn", onClick: () => threadClient.interrupt() })
      ),
    ];
  }

  render() {
    const { progress } = this.state;

    return div(
      { className: "webreplay-player" },
      div(
        { id: "overlay", className: "paused" },
        div(
          { className: "overlay-container " },
          div(
            { className: "commands" },
            ...this.renderCommands()
          ),
          div(
            { className: "progressBar" },
            div({
              className: "progress",
              style: { width: `${(Math.round((progress || 0) * 10000) / 100) + "%"}` },
            })
          )
        )
      )
    );
  }
}

module.exports = WebReplayPlayer;
