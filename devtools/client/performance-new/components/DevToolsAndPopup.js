/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @template P
 * @typedef {import("react-redux").ResolveThunks<P>} ResolveThunks<P>
 */

/**
 * @typedef {Object} StateProps
 * @property {boolean?} isSupportedPlatform
 * @property {PageContext} pageContext
 * @property {string | null} promptEnvRestart
 */

/**
 * @typedef {StateProps} Props
 * @typedef {import("../@types/perf").State} StoreState
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 * @typedef {import("../@types/perf").PageContext} PageContext
 */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  div,
  button,
} = require("devtools/client/shared/vendor/react-dom-factories");
const RecordingButton = createFactory(
  require("devtools/client/performance-new/components/RecordingButton.js")
);
const Settings = createFactory(
  require("devtools/client/performance-new/components/Settings.js")
);
const Description = createFactory(
  require("devtools/client/performance-new/components/Description.js")
);

const selectors = require("devtools/client/performance-new/store/selectors");
const {
  restartBrowserWithEnvironmentVariable,
} = require("devtools/client/performance-new/browser");

/**
 * This is the top level component for the DevTools panel and the profiler popup, but
 * not the about:profiling page. Eventually the profiler popup will use a different
 * implementation. See Bug 1597371 for more information about the migration plan.
 *
 * @extends {React.PureComponent<Props>}
 */
class DevToolsAndPopup extends PureComponent {
  /** @param {Props} props */
  constructor(props) {
    super(props);
    this.handleRestart = this.handleRestart.bind(this);
  }

  handleRestart() {
    const { promptEnvRestart } = this.props;
    if (!promptEnvRestart) {
      throw new Error(
        "handleRestart() should only be called when promptEnvRestart exists."
      );
    }
    restartBrowserWithEnvironmentVariable(promptEnvRestart, "1");
  }

  render() {
    const { isSupportedPlatform, pageContext, promptEnvRestart } = this.props;

    if (isSupportedPlatform === null) {
      // We don't know yet if this is a supported platform, wait for a response.
      return null;
    }

    return div(
      { className: `perf perf-${pageContext}` },
      promptEnvRestart
        ? div(
            { className: "perf-env-restart" },
            div(
              {
                className:
                  "perf-photon-message-bar perf-photon-message-bar-warning perf-env-restart-fixed",
              },
              div({ className: "perf-photon-message-bar-warning-icon" }),
              "The browser must be restarted to enable this feature.",
              button(
                {
                  className: "perf-photon-button perf-photon-button-micro",
                  type: "button",
                  onClick: this.handleRestart,
                },
                "Restart"
              )
            )
          )
        : null,
      RecordingButton(),
      Settings(),
      pageContext === "devtools" ? Description() : null
    );
  }
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
    pageContext: selectors.getPageContext(state),
    promptEnvRestart: selectors.getPromptEnvRestart(state),
  };
}

module.exports = connect(mapStateToProps)(DevToolsAndPopup);
