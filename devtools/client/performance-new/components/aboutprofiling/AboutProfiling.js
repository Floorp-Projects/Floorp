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
 * @typedef {import("../../@types/perf").State} StoreState
 * @typedef {import("../../@types/perf").PageContext} PageContext
 */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  div,
  h1,
  button,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const Localized = createFactory(
  require("resource://devtools/client/shared/vendor/fluent-react.js").Localized
);
const Settings = createFactory(
  require("resource://devtools/client/performance-new/components/aboutprofiling/Settings.js")
);
const Presets = createFactory(
  require("resource://devtools/client/performance-new/components/aboutprofiling/Presets.js")
);

const selectors = require("resource://devtools/client/performance-new/store/selectors.js");
const {
  restartBrowserWithEnvironmentVariable,
} = require("resource://devtools/client/performance-new/shared/browser.js");

/**
 * This is the top level component for the about:profiling page. It shares components
 * with the popup and DevTools page.
 *
 * @extends {React.PureComponent<Props>}
 */
class AboutProfiling extends PureComponent {
  handleRestart = () => {
    const { promptEnvRestart } = this.props;
    if (!promptEnvRestart) {
      throw new Error(
        "handleRestart() should only be called when promptEnvRestart exists."
      );
    }
    restartBrowserWithEnvironmentVariable(promptEnvRestart, "1");
  };

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
