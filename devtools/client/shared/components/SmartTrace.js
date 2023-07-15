/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");

const l10n = new LocalizationHelper(
  "devtools/client/locales/components.properties"
);
const dbgL10n = new LocalizationHelper(
  "devtools/client/locales/debugger.properties"
);
const Frames = createFactory(
  require("resource://devtools/client/debugger/src/components/SecondaryPanes/Frames/index.js")
    .Frames
);
const {
  annotateFramesWithLibrary,
} = require("resource://devtools/client/debugger/src/utils/pause/frames/annotateFrames.js");
const {
  getDisplayURL,
} = require("resource://devtools/client/debugger/src/utils/sources-tree/getURL.js");

class SmartTrace extends Component {
  static get propTypes() {
    return {
      stacktrace: PropTypes.array.isRequired,
      onViewSource: PropTypes.func.isRequired,
      onViewSourceInDebugger: PropTypes.func.isRequired,
      // Service to enable the source map feature.
      sourceMapURLService: PropTypes.object,
      // A number in ms (defaults to 100) which we'll wait before doing the first actual
      // render of this component, in order to avoid shifting layout rapidly in case the
      // page is using sourcemap.
      // Setting it to 0 or anything else than a number will force the first render to
      // happen immediatly, without any delay.
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
      ready: !props.sourceMapURLService || !this.hasInitialRenderDelay(),
      updateCount: 0,
      // Original positions for each indexed position
      originalLocations: null,
    };
  }

  getChildContext() {
    return { l10n: dbgL10n };
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    if (this.props.sourceMapURLService) {
      this.sourceMapURLServiceUnsubscriptions = [];
      const sourceMapInit = Promise.all(
        this.props.stacktrace.map(
          ({ filename, sourceId, lineNumber, columnNumber }, index) =>
            new Promise(resolve => {
              const callback = originalLocation => {
                this.onSourceMapServiceChange(originalLocation, index);
                resolve();
              };

              this.sourceMapURLServiceUnsubscriptions.push(
                this.props.sourceMapURLService.subscribeByLocation(
                  {
                    id: sourceId,
                    url: filename.split(" -> ").pop(),
                    line: lineNumber,
                    column: columnNumber,
                  },
                  callback
                )
              );
            })
        )
      );

      // Without initial render delay, we don't have to do anything; if the frames are
      // sourcemapped, we will get new renders from onSourceMapServiceChange.
      if (!this.hasInitialRenderDelay()) {
        return;
      }

      const delay = new Promise(res => {
        this.initialRenderDelayTimeoutId = setTimeout(
          res,
          this.props.initialRenderDelay
        );
      });

      // We wait either for the delay to be over (if it exists), or the sourcemapService
      // results to be available, before setting the state as initialized.
      Promise.race([delay, sourceMapInit]).then(() => {
        if (this.initialRenderDelayTimeoutId) {
          clearTimeout(this.initialRenderDelayTimeoutId);
        }
        this.setState(state => ({
          // Force-update so that the ready state is detected.
          updateCount: state.updateCount + 1,
          ready: true,
        }));
      });
    }
  }

  componentDidMount() {
    if (this.props.onReady && this.state.ready) {
      this.props.onReady();
    }
  }

  shouldComponentUpdate(_, nextState) {
    if (this.state.updateCount !== nextState.updateCount) {
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

    if (this.sourceMapURLServiceUnsubscriptions) {
      this.sourceMapURLServiceUnsubscriptions.forEach(unsubscribe => {
        unsubscribe();
      });
    }
  }

  componentDidCatch(error, info) {
    console.error(
      "Error while rendering stacktrace:",
      error,
      info,
      "props:",
      this.props
    );
    this.setState(state => ({
      // Force-update so the error is detected.
      updateCount: state.updateCount + 1,
      hasError: true,
    }));
  }

  onSourceMapServiceChange(originalLocation, index) {
    this.setState(({ originalLocations }) => {
      if (!originalLocations) {
        originalLocations = Array.from({
          length: this.props.stacktrace.length,
        });
      }
      return {
        originalLocations: [
          ...originalLocations.slice(0, index),
          originalLocation,
          ...originalLocations.slice(index + 1),
        ],
      };
    });

    if (this.onFrameLocationChangedTimeoutId) {
      clearTimeout(this.onFrameLocationChangedTimeoutId);
    }

    // Since a trace may have many original positions, we don't want to
    // constantly re-render every time one becomes available. To avoid this,
    // we only update the component after an initial timeout, and on a
    // debounce edge as more positions load after that.
    if (this.state.ready === true) {
      this.onFrameLocationChangedTimeoutId = setTimeout(() => {
        this.setState(state => ({
          updateCount: state.updateCount + 1,
        }));
      }, this.props.onSourceMapResultDebounceDelay);
    }
  }

  hasInitialRenderDelay() {
    return (
      Number.isFinite(this.props.initialRenderDelay) &&
      this.props.initialRenderDelay > 0
    );
  }

  render() {
    if (
      this.state.hasError ||
      (this.hasInitialRenderDelay() && !this.state.ready)
    ) {
      return null;
    }

    const { onViewSourceInDebugger, onViewSource, stacktrace } = this.props;
    const { originalLocations } = this.state;

    const frames = stacktrace.map(
      (
        {
          filename,
          sourceId,
          lineNumber,
          columnNumber,
          functionName,
          asyncCause,
        },
        i
      ) => {
        // Create partial debugger frontend "location" objects compliant with <Frames> react component requirements
        const sourceUrl = filename.split(" -> ").pop();
        const generatedLocation = {
          line: lineNumber,
          column: columnNumber,
          source: {
            // 'id' isn't used by Frames, but by selectFrame callback below
            id: sourceId,
            url: sourceUrl,
            // 'displayURL' might be used by FrameComponent via getFilename
            displayURL: getDisplayURL(sourceUrl),
          },
        };
        let location = generatedLocation;
        const originalLocation = originalLocations?.[i];
        if (originalLocation) {
          location = {
            line: originalLocation.line,
            column: originalLocation.column,
            source: {
              url: originalLocation.url,
              // 'displayURL' might be used by FrameComponent via getFilename
              displayURL: getDisplayURL(originalLocation.url),
            },
          };
        }

        // Create partial debugger frontend "frame" objects compliant with <Frames> react component requirements
        return {
          id: "fake-frame-id-" + i,
          displayName: functionName,
          asyncCause,
          location,
          // Note that for now, Frames component only uses 'location' attribute
          // and never the 'generatedLocation'.
          // But the code below does, the selectFrame callback.
          generatedLocation,
        };
      }
    );
    annotateFramesWithLibrary(frames);

    return Frames({
      frames,
      selectFrame: ({ generatedLocation }) => {
        const viewSource = onViewSourceInDebugger || onViewSource;

        viewSource({
          id: generatedLocation.source.id,
          url: generatedLocation.source.url,
          line: generatedLocation.line,
          column: generatedLocation.column,
        });
      },
      getFrameTitle: url => {
        return l10n.getFormatStr("frame.viewsourceindebugger", url);
      },
      disableFrameTruncate: true,
      disableContextMenu: true,
      frameworkGroupingOn: true,
      // Force displaying the original location (we might try to use current Debugger state?)
      shouldDisplayOriginalLocation: true,
      displayFullUrl: !this.state || !this.state.originalLocations,
      panel: "webconsole",
    });
  }
}

SmartTrace.childContextTypes = {
  l10n: PropTypes.object,
};

module.exports = SmartTrace;
