/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {sortBy} = require("devtools/client/shared/vendor/lodash");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);
const getFormatStr = (key, a) => L10N.getFormatStr(`toolbox.replay.${key}`, a);

const { div } = dom;

const markerWidth = 7;
const imgResource = "resource://devtools/client/debugger/new/images";
const shouldLog = false;

function classname(name, bools) {
  for (const key in bools) {
    if (bools[key]) {
      name += ` ${key}`;
    }
  }

  return name;
}

function log(message) {
  if (shouldLog) {
    console.log(message);
  }
}

function CommandButton({ img, className, onClick }) {
  const images = {
    rewind: "resume",
    resume: "resume",
    next: "next",
    previous: "next",
    pause: "pause",
    play: "resume",
  };

  return dom.div(
    {
      className: `command-button ${className}`,
      onClick,
    },
    dom.img({
      className: `btn ${img}`,
      style: {
        maskImage: `url("${imgResource}/${images[img]}.svg")`,
      },
    })
  );
}

function getMessageProgress(message) {
  return getProgress(message.executionPoint);
}

function getProgress(executionPoint) {
  return executionPoint && executionPoint.progress;
}

function getClosestMessage(messages, executionPoint) {
  const progress = getProgress(executionPoint);

  return sortBy(
    messages,
    message => Math.abs(progress - getMessageProgress(message))
  )[0];
}

/*
 *
 * The player has 4 valid states
 * - Paused:       (paused, !recording, !seeking)
 * - Playing:      (!paused, !recording, !seeking)
 * - Seeking:      (!paused, !recording, seeking)
 * - Recording:    (!paused, recording, !seeking)
 *
 */
class WebReplayPlayer extends Component {
  static get propTypes() {
    return {
      toolbox: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      executionPoint: null,
      recordingEndpoint: null,
      seeking: false,
      recording: true,
      paused: false,
      messages: [],
      highlightedMessage: null,
    };
    this.overlayWidth = 0;
  }

  componentDidMount() {
    this.overlayWidth = this.updateOverlayWidth();
    this.threadClient.addListener("paused", this.onPaused.bind(this));
    this.threadClient.addListener("resumed", this.onResumed.bind(this));
    this.threadClient.addListener("progress", this.onProgress.bind(this));

    this.toolbox.getPanelWhenReady("webconsole").then(panel => {
      const consoleFrame = panel.hud.ui;
      consoleFrame.on("message-hover", this.onConsoleMessageHover.bind(this));
      consoleFrame.consoleOutput.subscribeToStore(
        this.onConsoleUpdate.bind(this)
      );
    });
  }

  componentDidUpdate(prevProps, prevState) {
    this.overlayWidth = this.updateOverlayWidth();

    if (prevState.closestMessage != this.state.closestMessage) {
      this.scrollToMessage();
    }
  }

  get toolbox() {
    return this.props.toolbox;
  }

  get console() {
    return this.toolbox.getPanel("webconsole");
  }

  get threadClient() {
    return this.toolbox.threadClient;
  }

  isRecording() {
    return !this.isPaused() && this.state.recording;
  }

  isReplaying() {
    return !this.isPaused() && !this.state.recording;
  }

  isPaused() {
    return this.state.paused;
  }

  isSeeking() {
    return this.state.seeking;
  }

  onPaused(_, packet) {
    if (packet && packet.recordingEndpoint) {
      const { executionPoint, recordingEndpoint } = packet;
      const closestMessage = getClosestMessage(this.state.messages, executionPoint);

      this.setState({
        executionPoint,
        recordingEndpoint,
        paused: true,
        seeking: false,
        recording: false,
        closestMessage,
      });
    }
  }

  onResumed(_, packet) {
    this.setState({ paused: false, closestMessage: null });
  }

  onProgress(_, packet) {
    const { recording, executionPoint } = packet;
    log(`progress: ${recording ? "rec" : "play"} ${executionPoint.progress}`);

    if (this.state.seeking) {
      return;
    }

    // We want to prevent responding to interrupts
    if (this.isRecording() && !recording) {
      return;
    }

    const newState = { recording, executionPoint };
    if (recording) {
      newState.recordingEndpoint = executionPoint;
    }

    this.setState(newState);
  }

  onConsoleUpdate(consoleState) {
    const {
      messages: { visibleMessages, messagesById },
    } = consoleState;
    const messages = visibleMessages.map(id => messagesById.get(id));

    if (visibleMessages != this.state.visibleMessages) {
      this.setState({ messages, visibleMessages });
    }
  }

  onConsoleMessageHover(type, message) {
    if (type == "mouseleave") {
      return this.setState({ highlightedMessage: null });
    }

    if (type == "mouseenter") {
      return this.setState({ highlightedMessage: message.id });
    }

    return null;
  }

  scrollToMessage() {
    const {closestMessage} = this.state;

    if (!closestMessage) {
      return;
    }

    const consoleOutput = this.console.hud.ui.outputNode;
    const element =  consoleOutput
      .querySelector(`.message[data-message-id="${closestMessage.id}"]`);

    if (element) {
      const consoleHeight = consoleOutput.getBoundingClientRect().height;
      const elementTop = element.getBoundingClientRect().top;
      if (elementTop < 30 || elementTop + 50 > consoleHeight) {
        element.scrollIntoView({block: "center", behavior: "smooth"});
      }
    }
  }

  seek(executionPoint) {
    if (!executionPoint) {
      return null;
    }

    // set seeking to the current execution point to avoid a progress bar jump
    this.setState({ seeking: true });
    return this.threadClient.timeWarp(executionPoint);
  }

  next() {
    if (!this.isPaused()) {
      return null;
    }

    const { messages, executionPoint, recordingEndpoint } = this.state;
    let seekPoint = messages
      .map(m => m.executionPoint)
      .filter(point => point.progress > executionPoint.progress)
      .slice(0)[0];

    if (!seekPoint) {
      seekPoint = recordingEndpoint;
    }

    return this.seek(seekPoint);
  }

  async previous() {
    if (this.isRecording()) {
      await this.threadClient.interrupt();
    }

    if (!this.isPaused()) {
      return null;
    }

    const { messages, executionPoint } = this.state;

    const seekPoint = messages
      .map(m => m.executionPoint)
      .filter(point => point.progress < executionPoint.progress)
      .slice(-1)[0];

    return this.seek(seekPoint);
  }

  resume() {
    if (!this.isPaused()) {
      return null;
    }

    return this.threadClient.resume();
  }

  async rewind() {
    if (this.isRecording()) {
      await this.threadClient.interrupt();
    }

    if (!this.isPaused()) {
      return null;
    }

    return this.threadClient.rewind();
  }

  pause() {
    if (this.isPaused()) {
      return null;
    }

    return this.threadClient.interrupt();
  }

  renderCommands() {
    const paused = this.isPaused();
    const recording = this.isRecording();
    const seeking = this.isSeeking();

    return [
      CommandButton({
        className: classname("", { active: paused || recording }),
        img: "previous",
        onClick: () => this.previous(),
      }),

      CommandButton({
        className: classname("", { active: paused || recording }),
        img: "rewind",
        onClick: () => this.rewind(),
      }),

      CommandButton({
        className: classname(" primary", { active: !paused || seeking }),
        img: "pause",
        onClick: () => this.pause(),
      }),

      CommandButton({
        className: classname("", { active: paused }),
        img: "resume",
        onClick: () => this.resume(),
      }),

      CommandButton({
        className: classname("", { active: paused }),
        img: "next",
        onClick: () => this.next(),
      }),
    ];
  }

  updateOverlayWidth() {
    const el = ReactDOM.findDOMNode(this).querySelector(".progressBar");
    return el.clientWidth;
  }

  // calculate pixel distance from two points
  getDistanceFrom(to, from) {
    const toPercent = this.getPercent(to);
    const fromPercent = this.getPercent(from);

    return ((toPercent - fromPercent) * this.overlayWidth) / 100;
  }

  getOffset(point) {
    const percent = this.getPercent(point);
    return (percent * this.overlayWidth) / 100;
  }

  renderMessage(message, index) {
    const { messages, executionPoint, highlightedMessage } = this.state;

    const offset = this.getOffset(message.executionPoint);
    const previousMessage = messages[index - 1];

    // Check to see if two messages overlay each other on the timeline
    const isOverlayed =
      previousMessage &&
      this.getDistanceFrom(
        message.executionPoint,
        previousMessage.executionPoint
      ) < markerWidth;

    // Check to see if a message appears after the current execution point
    const isFuture =
      this.getDistanceFrom(message.executionPoint, executionPoint) >
      markerWidth / 2;

    const isHighlighted = highlightedMessage == message.id;

    return dom.a({
      className: classname("message", {
        overlayed: isOverlayed,
        future: isFuture,
        highlighted: isHighlighted,
      }),
      style: {
        left: `${offset - markerWidth / 2}px`,
        zIndex: `${index + 100}`,
      },
      title: getFormatStr("jumpMessage", index + 1),
      onClick: () => this.seek(message.executionPoint),
    });
  }

  renderMessages() {
    const messages = this.state.messages;
    return messages.map((message, index) => this.renderMessage(message, index));
  }

  getPercent(executionPoint) {
    if (!this.state.recordingEndpoint) {
      return 100;
    }

    if (!executionPoint) {
      return 0;
    }

    const ratio =
      executionPoint.progress / this.state.recordingEndpoint.progress;
    return ratio * 100;
  }

  render() {
    const percent = this.getPercent(this.state.executionPoint);
    const recording = this.isRecording();
    return div(
      { className: "webreplay-player" },
      div(
        {
          id: "overlay",
          className: classname("", { recording: recording, paused: !recording }),
        },
        div(
          { className: "overlay-container " },
          div({ className: "commands" }, ...this.renderCommands()),
          div(
            { className: "progressBar" },
            div({
              className: "progress",
              style: { width: `${percent}%` },
            }),
            div({
              className: "progress-line",
              style: { width: `${percent}%` },
            }),
            div({
              className: "progress-line end",
              style: { left: `${percent}%`, width: `${100 - percent}%` },
            }),
            ...this.renderMessages()
          )
        )
      )
    );
  }
}

module.exports = WebReplayPlayer;
