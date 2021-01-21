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
 * @property {(() => void) | undefined} openRemoteDevTools
 */

/**
 * @typedef {StateProps} Props
 * @typedef {import("../@types/perf").State} StoreState
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 *@typedef {import("../@types/perf").PageContext} PageContext
 */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  div,
  h1,
  button,
} = require("devtools/client/shared/vendor/react-dom-factories");
const Localized = createFactory(
  require("devtools/client/shared/vendor/fluent-react").Localized
);
const Settings = createFactory(
  require("devtools/client/performance-new/components/Settings.js")
);
const Presets = createFactory(
  require("devtools/client/performance-new/components/Presets")
);

const selectors = require("devtools/client/performance-new/store/selectors");
const {
  restartBrowserWithEnvironmentVariable,
} = require("devtools/client/performance-new/browser");

/**
 * This is the top level component for the about:profiling page. It shares components
 * with the popup and DevTools page.
 *
 * @extends {React.PureComponent<Props>}
 */
class AboutProfiling extends PureComponent {
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
    const {
      isSupportedPlatform,
      pageContext,
      promptEnvRestart,
      openRemoteDevTools,
    } = this.props;

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
              Localized({ id: "perftools-status-restart-required" }),
              button(
                {
                  className: "perf-photon-button perf-photon-button-micro",
                  type: "button",
                  onClick: this.handleRestart,
                },
                Localized({ id: "perftools-button-restart" })
              )
            )
          )
        : null,

      openRemoteDevTools
        ? div(
            { className: "perf-back" },
            button(
              {
                className: "perf-back-button",
                type: "button",
                onClick: openRemoteDevTools,
              },
              Localized({ id: "perftools-button-save-settings" })
            )
          )
        : null,

      div(
        { className: "perf-intro" },
        h1(
          { className: "perf-intro-title" },
          Localized({ id: "perftools-intro-title" })
        ),
        div(
          { className: "perf-intro-row" },
          div({}, div({ className: "perf-intro-icon" })),
          Localized({
            className: "perf-intro-text",
            id: "perftools-intro-description",
          })
        )
      ),
      Presets(),
      Settings()
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
    openRemoteDevTools: selectors.getOpenRemoteDevTools(state),
  };
}

module.exports = connect(mapStateToProps)(AboutProfiling);
