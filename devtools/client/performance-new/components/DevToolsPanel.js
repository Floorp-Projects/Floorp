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
 * @typedef {StateProps} Props
 * @typedef {import("../@types/perf").State} StoreState
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  div,
  hr,
} = require("devtools/client/shared/vendor/react-dom-factories");
const RecordingButton = createFactory(
  require("devtools/client/performance-new/components/RecordingButton")
);
const Description = createFactory(
  require("devtools/client/performance-new/components/Description")
);
const DevToolsPresetSelection = createFactory(
  require("devtools/client/performance-new/components/DevToolsPresetSelection")
);
const OnboardingMessage = createFactory(
  require("devtools/client/performance-new/components/OnboardingMessage")
);

const selectors = require("devtools/client/performance-new/store/selectors");

/**
 * This is the top level component for the DevTools panel.
 *
 * @extends {React.PureComponent<Props>}
 */
class DevToolsPanel extends PureComponent {
  render() {
    const { isSupportedPlatform } = this.props;

    if (isSupportedPlatform === null) {
      // We don't know yet if this is a supported platform, wait for a response.
      return null;
    }

    return div(
      { className: `perf perf-devtools` },
      OnboardingMessage(),
      RecordingButton(),
      Description(),
      hr({ className: "perf-presets-hr" }),
      DevToolsPresetSelection()
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
  };
}

module.exports = connect(mapStateToProps)(DevToolsPanel);
