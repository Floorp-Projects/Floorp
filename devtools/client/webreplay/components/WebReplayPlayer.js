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
      toolbox: PropTypes.object
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      executionPoint: null,
      recordingEndpoint: null,
      seeking: false,
      messages: []
    };
  }

  componentDidMount() {
    this.threadClient.addListener("paused", this.onPaused.bind(this));
    this.threadClient.addListener("resumed", this.onResumed.bind(this));
    this.activeConsole._client.addListener(
      "consoleAPICall",
      this.onMessage.bind(this)
    );
  }

  get threadClient() {
    return this.props.toolbox.threadClient;
  }

  get activeConsole() {
    return this.props.toolbox.target.activeConsole;
  }

  isPaused() {
    const { executionPoint, seeking } = this.state;
    return !!executionPoint || !!seeking;
  }

  onPaused(_, packet) {
    if (packet && packet.recordingEndpoint) {
      const { executionPoint, recordingEndpoint } = packet;
      this.setState({ executionPoint, recordingEndpoint, seeking: false });
    }
  }

  onResumed(_, packet) {
    this.setState({ executionPoint: null });
  }

  onMessage(_, packet) {
    this.setState({ messages: this.state.messages.concat(packet.message) });
  }

  seek(executionPoint) {
    if (!executionPoint) {
      return null;
    }

    // set seeking to the current execution point to avoid a progress bar jump
    this.setState({ seeking: this.state.executionPoint });
    return this.threadClient.timeWarp(executionPoint);
  }

  next(ev) {
    if (!ev.metaKey) {
      return this.threadClient.resume();
    }

    const { messages, executionPoint } = this.state;
    const seekPoint = messages
      .map(m => m.executionPoint)
      .filter(point => point.progress > executionPoint.progress)
      .slice(0)[0];

    return this.seek(seekPoint);
  }

  previous(ev) {
    if (!ev.metaKey) {
      return this.threadClient.rewind();
    }

    const { messages, executionPoint } = this.state;

    const seekPoint = messages
      .map(m => m.executionPoint)
      .filter(point => point.progress < executionPoint.progress)
      .slice(-1)[0];

    return this.seek(seekPoint);
  }

  renderCommands() {
    if (this.isPaused()) {
      return [
        div(
          { className: "command-button" },
          div({
            className: "rewind-button btn",
            onClick: ev => this.previous(ev)
          })
        ),
        div(
          { className: "command-button" },
          div({
            className: "play-button btn",
            onClick: ev => this.next(ev)
          })
        )
      ];
    }

    return [
      div(
        { className: "command-button" },
        div({
          className: "pause-button btn",
          onClick: () => this.threadClient.interrupt()
        })
      )
    ];
  }

  renderMessages() {
    const messages = this.state.messages;

    return messages.map((message, index) =>
      dom.div({
        className: "message",
        style: {
          left: `${this.getPercent(message.executionPoint)}%`
        },
        onClick: () => this.seek(message.executionPoint)
      })
    );
  }

  getPercent(executionPoint) {
    if (!executionPoint || !this.state.recordingEndpoint) {
      return 0;
    }

    const ratio =
      executionPoint.progress / this.state.recordingEndpoint.progress;
    return Math.round(ratio * 100);
  }

  render() {
    return div(
      { className: "webreplay-player" },
      div(
        { id: "overlay", className: "paused" },
        div(
          { className: "overlay-container " },
          div({ className: "commands" }, ...this.renderCommands()),
          div(
            { className: "progressBar" },
            div({
              className: "progress",
              style: {
                width: `${this.getPercent(
                  this.state.executionPoint || this.state.seeking
                )}%`
              }
            }),
            ...this.renderMessages()
          )
        )
      )
    );
  }
}

module.exports = WebReplayPlayer;
