/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { sortBy, range } = require("devtools/client/shared/vendor/lodash");

ChromeUtils.defineModuleGetter(
  this,
  "pointEquals",
  "resource://devtools/shared/execution-point-utils.js"
);

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);
const getFormatStr = (key, a) => L10N.getFormatStr(`toolbox.replay.${key}`, a);

const { div } = dom;

const markerWidth = 7;
const imgResource = "resource://devtools/client/debugger/images";
const imgChrome = "chrome://devtools/skin/images";
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

function CommandButton({ img, className, onClick, active }) {
  const images = {
    rewind: "replay-resume",
    resume: "replay-resume",
    next: "next",
    previous: "next",
    pause: "replay-pause",
    play: "replay-resume",
  };

  const filename = images[img];
  const path = filename == "next" ? imgChrome : imgResource;
  const attrs = {
    className: classname(`command-button ${className}`, { active }),
    onClick,
  };

  if (active) {
    attrs.title = L10N.getStr(`toolbox.replay.${img}`);
  }

  return dom.div(
    attrs,
    dom.img({
      className: `btn ${img} ${className}`,
      style: {
        maskImage: `url("${path}/${filename}.svg")`,
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

  return sortBy(messages, message =>
    Math.abs(progress - getMessageProgress(message))
  )[0];
}

function sameLocation(m1, m2) {
  const f1 = m1.frame;
  const f2 = m2.frame;

  return (
    f1.source === f2.source && f1.line === f2.line && f1.column === f2.column
  );
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
      unscannedRegions: [],
      cachedPoints: [],
      start: 0,
      end: 1,
    };

    this.lastPaint = null;
    this.overlayWidth = 1;

    this.onProgressBarClick = this.onProgressBarClick.bind(this);
    this.onProgressBarMouseOver = this.onProgressBarMouseOver.bind(this);
    this.onPlayerMouseLeave = this.onPlayerMouseLeave.bind(this);
  }

  componentDidMount() {
    this.overlayWidth = this.updateOverlayWidth();
    this.threadFront.on("paused", this.onPaused.bind(this));
    this.threadFront.on("resumed", this.onResumed.bind(this));
    this.threadFront.on("progress", this.onProgress.bind(this));

    this.toolbox.getPanelWhenReady("webconsole").then(panel => {
      const consoleFrame = panel.hud.ui;
      consoleFrame.on("message-hover", this.onConsoleMessageHover.bind(this));
      consoleFrame.wrapper.subscribeToStore(this.onConsoleUpdate.bind(this));
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

  get threadFront() {
    return this.toolbox.threadFront;
  }

  isCached(message) {
    return this.state.cachedPoints.includes(message.executionPoint.progress);
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

  getTickSize() {
    const { start, end } = this.state;
    const minSize = 10;

    if (!start && !end) {
      return minSize;
    }

    const maxSize = this.overlayWidth / 10;
    const ratio = end - start;
    return (1 - ratio) * maxSize + minSize;
  }

  getClosestMessage(point) {
    return getClosestMessage(this.state.messages, point);
  }

  getMousePosition(e) {
    const { start, end } = this.state;

    const { left, width } = e.currentTarget.getBoundingClientRect();
    const clickLeft = e.clientX;

    const clickPosition = (clickLeft - left) / width;
    return (end - start) * clickPosition + start;
  }

  paint(point) {
    if (this.lastPaint !== point) {
      this.lastPaint = point;
      this.threadFront.paint(point);
    }
  }

  onPaused(packet) {
    if (packet && packet.recordingEndpoint) {
      const { executionPoint, recordingEndpoint } = packet;
      const closestMessage = this.getClosestMessage(executionPoint);

      const pausedMessage = this.state.messages.find(message =>
        pointEquals(message.executionPoint, executionPoint)
      );

      this.setState({
        executionPoint,
        recordingEndpoint,
        paused: true,
        seeking: false,
        recording: false,
        closestMessage,
        pausedMessage,
      });
    }
  }

  onResumed(packet) {
    this.setState({ paused: false, closestMessage: null, pausedMessage: null });
  }

  onProgress(packet) {
    const {
      recording,
      executionPoint,
      unscannedRegions,
      cachedPoints,
    } = packet;
    log(`progress: ${recording ? "rec" : "play"} ${executionPoint.progress}`);

    if (this.state.seeking) {
      return;
    }

    // We want to prevent responding to interrupts
    if (this.isRecording() && !recording) {
      return;
    }

    const newState = {
      recording,
      executionPoint,
      unscannedRegions,
      cachedPoints,
    };
    if (recording) {
      newState.recordingEndpoint = executionPoint;
    }

    this.setState(newState);
  }

  onConsoleUpdate(consoleState) {
    const {
      messages: { visibleMessages, messagesById },
    } = consoleState;

    if (visibleMessages != this.state.visibleMessages) {
      let messages = visibleMessages
        .map(id => messagesById.get(id))
        .filter(message => message.source == "console-api");

      messages = sortBy(messages, message => getMessageProgress(message));

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

  setTimelinePosition({ position, direction }) {
    this.setState({ [direction]: position });
  }

  scrollToMessage() {
    const { closestMessage } = this.state;

    if (!closestMessage) {
      return;
    }

    const consoleOutput = this.console.hud.ui.outputNode;
    const element = consoleOutput.querySelector(
      `.message[data-message-id="${closestMessage.id}"]`
    );

    if (element) {
      const consoleHeight = consoleOutput.getBoundingClientRect().height;
      const elementTop = element.getBoundingClientRect().top;
      if (elementTop < 30 || elementTop + 50 > consoleHeight) {
        element.scrollIntoView({ block: "center", behavior: "smooth" });
      }
    }
  }

  onMessageMouseEnter(executionPoint) {
    return this.paint(executionPoint);
  }

  onProgressBarClick(e) {
    if (!e.altKey) {
      return;
    }

    const direction = e.shiftKey ? "end" : "start";
    const position = this.getMousePosition(e);
    this.setTimelinePosition({ position, direction });
  }

  onProgressBarMouseOver(e) {
    const mousePosition = this.getMousePosition(e) * 100;

    const closestMessage = sortBy(this.state.messages, message =>
      Math.abs(this.getVisiblePercent(message.executionPoint) - mousePosition)
    ).filter(message => this.isCached(message))[0];

    if (!closestMessage) {
      return;
    }

    this.paint(closestMessage.executionPoint);
  }

  onPlayerMouseLeave() {
    return this.threadFront.paintCurrentPoint();
  }

  seek(executionPoint) {
    if (!executionPoint) {
      return null;
    }

    // set seeking to the current execution point to avoid a progress bar jump
    this.setState({ seeking: true });
    return this.threadFront.timeWarp(executionPoint);
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
      await this.threadFront.interrupt();
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

    return this.threadFront.resume();
  }

  async rewind() {
    if (this.isRecording()) {
      await this.threadFront.interrupt();
    }

    if (!this.isPaused()) {
      return null;
    }

    return this.threadFront.rewind();
  }

  pause() {
    if (this.isPaused()) {
      return null;
    }

    return this.threadFront.interrupt();
  }

  renderCommands() {
    const paused = this.isPaused();
    const recording = this.isRecording();
    const seeking = this.isSeeking();

    return [
      CommandButton({
        className: "",
        active: paused || recording,
        img: "rewind",
        onClick: () => this.rewind(),
      }),

      CommandButton({
        className: "primary",
        active: !paused || seeking,
        img: "pause",
        onClick: () => this.pause(),
      }),

      CommandButton({
        className: "",
        active: paused,
        img: "resume",
        onClick: () => this.resume(),
      }),
    ];
  }

  updateOverlayWidth() {
    const el = ReactDOM.findDOMNode(this).querySelector(".progressBar");
    return el ? el.clientWidth : 1;
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

  getPercent(executionPoint) {
    const { recordingEndpoint } = this.state;

    if (!recordingEndpoint) {
      return 100;
    }

    if (!executionPoint) {
      return 0;
    }

    const ratio = executionPoint.progress / recordingEndpoint.progress;
    return ratio * 100;
  }

  getVisiblePercent(executionPoint) {
    const { start, end } = this.state;

    const position = this.getPercent(executionPoint) / 100;

    if (position < start || position > end) {
      return -1;
    }

    return ((position - start) / (end - start)) * 100;
  }

  getVisibleOffset(point) {
    const percent = this.getVisiblePercent(point);
    return (percent * this.overlayWidth) / 100;
  }

  renderMessage(message, index) {
    const {
      messages,
      executionPoint,
      pausedMessage,
      highlightedMessage,
    } = this.state;

    const offset = this.getVisibleOffset(message.executionPoint);
    const previousMessage = messages[index - 1];

    if (offset < 0) {
      return null;
    }

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

    const uncached = message.executionPoint && !this.isCached(message);

    const atPausedLocation =
      pausedMessage && sameLocation(pausedMessage, message);

    const { source, line, column } = message.frame;
    const filename = source.split("/").pop();
    let frameLocation = `${filename}:${line}`;
    if (column > 100) {
      frameLocation += `:${column}`;
    }

    return dom.a({
      className: classname("message", {
        overlayed: isOverlayed,
        future: isFuture,
        highlighted: isHighlighted,
        uncached,
        location: atPausedLocation,
      }),
      style: {
        left: `${Math.max(offset - markerWidth / 2, 0)}px`,
        zIndex: `${index + 100}`,
      },
      title: getFormatStr("jumpMessage2", frameLocation),
      onClick: e => {
        e.preventDefault();
        e.stopPropagation();
        this.seek(message.executionPoint);
      },
      onMouseEnter: () => this.onMessageMouseEnter(message.executionPoint),
    });
  }

  renderMessages() {
    const messages = this.state.messages;
    return messages.map((message, index) => this.renderMessage(message, index));
  }

  renderTicks() {
    const tickSize = this.getTickSize();
    const ticks = Math.round(this.overlayWidth / tickSize);
    return range(ticks).map((value, index) => this.renderTick(index));
  }

  renderTick(index) {
    const { executionPoint } = this.state;
    const tickSize = this.getTickSize();
    const offset = Math.round(this.getOffset(executionPoint));
    const position = index * tickSize;
    const isFuture = position > offset;

    return dom.span({
      className: classname("tick", {
        future: isFuture,
      }),
      style: {
        left: `${position}px`,
        width: `${tickSize}px`,
      },
    });
  }

  renderUnscannedRegions() {
    return this.state.unscannedRegions.map(
      this.renderUnscannedRegion.bind(this)
    );
  }

  renderUnscannedRegion({ start, end }) {
    let startOffset = this.getVisibleOffset({ progress: start });
    let endOffset = this.getVisibleOffset({ progress: end });

    if (startOffset > this.overlayWidth || endOffset < 0) {
      return null;
    }

    if (startOffset < 0) {
      startOffset = 0;
    }

    if (endOffset > this.overlayWidth) {
      endOffset = this.overlayWidth;
    }

    return dom.span({
      className: classname("unscanned"),
      style: {
        left: `${startOffset}px`,
        width: `${endOffset - startOffset}px`,
      },
    });
  }

  render() {
    const percent = this.getVisiblePercent(this.state.executionPoint);
    const recording = this.isRecording();
    return div(
      { className: "webreplay-player" },
      div(
        {
          id: "overlay",
          className: classname("", {
            recording: recording,
            paused: !recording,
          }),
          onMouseLeave: this.onPlayerMouseLeave,
        },
        div(
          { className: "overlay-container " },
          div({ className: "commands" }, ...this.renderCommands()),
          div(
            {
              className: "progressBar",
              onClick: this.onProgressBarClick,
              onDoubleClick: () => this.setState({ start: 0, end: 1 }),
              onMouseOver: this.onProgressBarMouseOver,
            },
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
            ...this.renderMessages(),
            ...this.renderTicks(),
            ...this.renderUnscannedRegions()
          )
        )
      )
    );
  }
}

module.exports = WebReplayPlayer;
