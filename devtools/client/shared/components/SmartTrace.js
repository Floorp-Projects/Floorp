/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { LocalizationHelper } = require("devtools/shared/l10n");

const l10n = new LocalizationHelper("devtools/client/locales/components.properties");
const dbgL10n = new LocalizationHelper("devtools/client/locales/debugger.properties");
const Frames = createFactory(require("devtools/client/debugger/src/components/SecondaryPanes/Frames/index").Frames);
const { annotateFrames } = require("devtools/client/debugger/src/utils/pause/frames/annotateFrames");

class SmartTrace extends Component {
  static get propTypes() {
    return {
      stacktrace: PropTypes.array.isRequired,
      onViewSource: PropTypes.func.isRequired,
      onViewSourceInDebugger: PropTypes.func.isRequired,
      onViewSourceInScratchpad: PropTypes.func,
      // Service to enable the source map feature.
      sourceMapService: PropTypes.object,
      initialRenderDelay: PropTypes.number,
      onSourceMapResultDebounceDelay: PropTypes.number,
      // Function that will be called when the SmartTrace is ready, i.e. once it was
      // rendered.
      onReady: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      initialRenderDelay: 100,
      onSourceMapResultDebounceDelay: 200,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      hasError: false,
      // If a sourcemap service is passed, we want to introduce a small delay in rendering
      // so we can have the results from the sourcemap service, or render if they're not
      // available yet.
      ready: !props.sourceMapService,
      frozen: false,
    };
  }

  getChildContext() {
    return { l10n: dbgL10n };
  }

  componentWillMount() {
    if (this.props.sourceMapService) {
      this.sourceMapServiceUnsubscriptions = [];
      const subscriptions = this.props.stacktrace.map((frame, index) =>
        new Promise(resolve => {
          const { lineNumber, columnNumber, filename } = frame;
          const source = filename.split(" -> ").pop();
          const subscribeCallback = (isSourceMapped, url, line, column) => {
            this.onSourceMapServiceChange(isSourceMapped, url, line, column, index);
            resolve();
          };
          const unsubscribe = this.props.sourceMapService.subscribe(
            source, lineNumber, columnNumber, subscribeCallback);
          this.sourceMapServiceUnsubscriptions.push(unsubscribe);
        }));

      const delay = new Promise(res => {
        this.initialRenderDelayTimeoutId = setTimeout(res, this.props.initialRenderDelay);
      });

      const sourceMapInit = Promise.all(subscriptions);

      // We wait either for the delay to be other or the sourcemapService results to
      // be available before setting the state as initialized.
      Promise.race([delay, sourceMapInit]).then(() => {
        if (this.initialRenderDelayTimeoutId) {
          clearTimeout(this.initialRenderDelayTimeoutId);
        }
        this.setState(state => ({...state, ready: true}));
      });
    }
  }

  componentDidMount() {
    if (this.props.onReady && this.state.ready) {
      this.props.onReady();
    }
  }

  shouldComponentUpdate(_, nextState) {
    if (this.state.ready === false && nextState.ready === true) {
      return true;
    }

    if (this.state.ready && this.state.frozen && !nextState.frozen) {
      return true;
    }

    return false;
  }

  componentDidUpdate(_, previousState) {
    if (this.props.onReady && !previousState.ready && this.state.ready) {
      this.props.onReady();
    }
  }

  componentWillUnmount() {
    if (this.initialRenderDelayTimeoutId) {
      clearTimeout(this.initialRenderDelayTimeoutId);
    }

    if (this.onFrameLocationChangedTimeoutId) {
      clearTimeout(this.initialRenderDelayTimeoutId);
    }

    if (this.sourceMapServiceUnsubscriptions) {
      this.sourceMapServiceUnsubscriptions.forEach(unsubscribe => {
        if (typeof unsubscribe === "function") {
          unsubscribe();
        }
      });
    }
  }

  componentDidCatch(error, info) {
    console.error("Error while rendering stacktrace:", error, info, "props:", this.props);
    this.setState({ hasError: true });
  }

  onSourceMapServiceChange(isSourceMapped, filename, lineNumber, columnNumber, index) {
    if (isSourceMapped) {
      if (this.onFrameLocationChangedTimeoutId) {
        clearTimeout(this.onFrameLocationChangedTimeoutId);
      }

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
          frozen: true,
          stacktrace: newStacktrace,
        };
      });

      // We only want to have a pending timeout if the component is ready.
      if (this.state.ready === true) {
        this.onFrameLocationChangedTimeoutId = setTimeout(() => {
          this.setState(state => ({
            ...state,
            frozen: false,
          }));
        }, this.props.onSourceMapResultDebounceDelay);
      }
    }
  }

  render() {
    if (this.state.hasError || (
      Number.isFinite(this.props.initialRenderDelay) &&
      !this.state.ready
    )) {
      return null;
    }

    const {
      onViewSourceInDebugger,
      onViewSource,
      onViewSourceInScratchpad,
    } = this.props;

    const stacktrace = this.state.isSourceMapped
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
      selectFrame: (cx, {location}) => {
        const viewSource = /^Scratchpad\/\d+$/.test(location.filename)
          ? onViewSourceInScratchpad
          : onViewSourceInDebugger || onViewSource;

        viewSource({
          sourceId: location.sourceId,
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
      selectable: true,
    });
  }
}

SmartTrace.childContextTypes = {
  l10n: PropTypes.object,
};

module.exports = SmartTrace;
