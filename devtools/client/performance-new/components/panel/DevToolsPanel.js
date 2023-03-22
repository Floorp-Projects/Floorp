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
 */

/**
 * @typedef {Object} OwnProps
 * @property {import("../../@types/perf").PerfFront} perfFront
 * @property {import("../../@types/perf").OnProfileReceived} onProfileReceived
 * @property {() => void} onEditSettingsLinkClicked
 */

/**
 * @typedef {StateProps & OwnProps} Props
 * @typedef {import("../../@types/perf").State} StoreState
 * @typedef {import("../../@types/perf").RecordingState} RecordingState
 * @typedef {import("../../@types/perf").PanelWindow} PanelWindow
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
  hr,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const RecordingButton = createFactory(
  require("resource://devtools/client/performance-new/components/panel/RecordingButton.js")
);
const Description = createFactory(
  require("resource://devtools/client/performance-new/components/panel/Description.js")
);
const DevToolsPresetSelection = createFactory(
  require("resource://devtools/client/performance-new/components/panel/DevToolsPresetSelection.js")
);
const OnboardingMessage = createFactory(
  require("resource://devtools/client/performance-new/components/panel/OnboardingMessage.js")
);
const ToolboxHighlightController = createFactory(
  require("resource://devtools/client/performance-new/components/panel/ToolboxHighlightController.js")
);

const selectors = require("resource://devtools/client/performance-new/store/selectors.js");
const anyWindow = /** @type {any} */ (window);
const panelWindow = /** @type {PanelWindow} */ (anyWindow);

/**
 * This is the top level component for the DevTools panel.
 *
 * @extends {React.PureComponent<Props>}
 */
class DevToolsPanel extends PureComponent {
  render() {
    const {
      isSupportedPlatform,
      perfFront,
      onProfileReceived,
      onEditSettingsLinkClicked,
    } = this.props;

    if (isSupportedPlatform === null) {
      // We don't know yet if this is a supported platform, wait for a response.
      return null;
    }

    return [
      OnboardingMessage(),
      div(
        { className: `perf perf-devtools` },
        RecordingButton({ perfFront, onProfileReceived }),
        panelWindow.gToolbox
          ? ToolboxHighlightController({ toolbox: panelWindow.gToolbox })
          : null,
        Description(),
        hr({ className: "perf-presets-hr" }),
        DevToolsPresetSelection({ onEditSettingsLinkClicked })
      ),
    ];
  }
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
  };
}

module.exports = connect(mapStateToProps)(DevToolsPanel);
