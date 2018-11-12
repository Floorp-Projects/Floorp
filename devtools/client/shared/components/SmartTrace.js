/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { LocalizationHelper } = require("devtools/shared/l10n");

const l10n = new LocalizationHelper("devtools/client/locales/components.properties");
const dbgL10n = new LocalizationHelper("devtools/client/locales/debugger.properties");
const Frames = createFactory(require("devtools/client/debugger/new/src/components/SecondaryPanes/Frames/index").Frames);
const { annotateFrames } = require("devtools/client/debugger/new/src/utils/pause/frames/annotateFrames");

class SmartTrace extends Component {
  static get propTypes() {
    return {
      stacktrace: PropTypes.array.isRequired,
      onViewSource: PropTypes.func.isRequired,
      onViewSourceInDebugger: PropTypes.func.isRequired,
      onViewSourceInScratchpad: PropTypes.func,
      // Service to enable the source map feature.
      sourceMapService: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.onSourceMapServiceChange = this.onSourceMapServiceChange.bind(this);
    this.state = { hasError: false };
  }

  getChildContext() {
    return { l10n: dbgL10n };
  }

  componentWillMount() {
    if (this.props.sourceMapService) {
      this.sourceMapServiceUnsubscriptions = this.props.stacktrace.map((frame, index) => {
        const { lineNumber, columnNumber, filename } = frame;
        const source = filename.split(" -> ").pop();
        return this.props.sourceMapService.subscribe(source, lineNumber, columnNumber,
          (isSourceMapped, url, line, column) =>
            this.onSourceMapServiceChange(isSourceMapped, url, line, column, index));
      });
    }
  }

  componentWillUnmount() {
    if (this.sourceMapServiceUnsubscriptions) {
      this.sourceMapServiceUnsubscriptions.forEach(unsubscribe => {
        if (typeof unsubscribe === "function") {
          unsubscribe();
        }
      });
    }
  }

  onSourceMapServiceChange(isSourceMapped, filename, lineNumber, columnNumber, index) {
    if (isSourceMapped) {
      this.setState(state => {
        const stacktrace = ((state && state.stacktrace) || this.props.stacktrace);
        const frame = stacktrace[index];

        const newStacktrace = stacktrace.slice(0, index).concat({
          ...frame,
          filename,
          lineNumber,
          columnNumber,
        }).concat(stacktrace.slice(index + 1));

        return {
          isSourceMapped: true,
          stacktrace: newStacktrace,
        };
      });
    }
  }

  componentDidCatch(error, info) {
    console.error("Error while rendering stacktrace:", error, info, "props:", this.props);
    this.setState({ hasError: true });
  }

  render() {
    if (this.state && this.state.hasError) {
      return null;
    }

    const {
      onViewSourceInDebugger,
      onViewSource,
      onViewSourceInScratchpad,
    } = this.props;

    const stacktrace = this.state && this.state.isSourceMapped
      ? this.state.stacktrace
      : this.props.stacktrace;

    const frames = annotateFrames(stacktrace.map((f, i) => ({
      ...f,
      id: "fake-frame-id-" + i,
      location: {
        ...f,
        line: f.lineNumber,
        column: f.columnNumber,
      },
      displayName: f.functionName,
      source: {
        url: f.filename && f.filename.split(" -> ").pop(),
      },
    })));

    return Frames({
      frames,
      selectFrame: ({location}) => {
        const viewSource = /^Scratchpad\/\d+$/.test(location.filename)
          ? onViewSourceInScratchpad
          : onViewSourceInDebugger || onViewSource;

        viewSource({
          url: location.filename,
          line: location.line,
          column: location.column,
        });
      },
      getFrameTitle: url => {
        return l10n.getFormatStr("frame.viewsourceindebugger", url);
      },
      disableFrameTruncate: true,
      disableContextMenu: true,
      frameworkGroupingOn: true,
      displayFullUrl: !this.state || !this.state.isSourceMapped,
    });
  }
}

SmartTrace.childContextTypes = {
  l10n: PropTypes.object,
};

module.exports = SmartTrace;
