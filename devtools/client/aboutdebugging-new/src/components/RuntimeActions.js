/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const ConnectionPromptSetting = createFactory(require("./ConnectionPromptSetting"));
const ExtensionDebugSetting = createFactory(require("./ExtensionDebugSetting"));
const TemporaryExtensionInstaller =
  createFactory(require("./debugtarget/TemporaryExtensionInstaller"));

const Actions = require("../actions/index");
const { DEBUG_TARGET_PANE, RUNTIMES } = require("../constants");
const Types = require("../types/index");

const {
  isExtensionDebugSettingNeeded,
  isSupportedDebugTargetPane,
} = require("../modules/debug-target-support");

class RuntimeActions extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      runtimeDetails: Types.runtimeDetails,
      runtimeId: PropTypes.string.isRequired,
    };
  }

  onProfilerButtonClick() {
    this.props.dispatch(Actions.showProfilerDialog());
  }

  renderConnectionPromptSetting() {
    const { dispatch, runtimeDetails, runtimeId } = this.props;
    const { connectionPromptEnabled } = runtimeDetails;
    // do not show the connection prompt setting in 'This Firefox'
    return runtimeId !== RUNTIMES.THIS_FIREFOX
             ? ConnectionPromptSetting({
               className: "runtime-actions__end",
               connectionPromptEnabled,
               dispatch,
             })
             : null;
  }

  renderExtensionDebugSetting() {
    const { dispatch, runtimeDetails } = this.props;
    const { extensionDebugEnabled, info } = runtimeDetails;
    return isExtensionDebugSettingNeeded(info.type)
             ? ExtensionDebugSetting({
                 className: "runtime-actions__start",
                 dispatch,
                 extensionDebugEnabled,
             })
             : null;
  }

  renderProfileButton() {
    const { runtimeId } = this.props;

    return runtimeId !== RUNTIMES.THIS_FIREFOX
         ? Localized(
           {
             id: "about-debugging-runtime-profile-button",
           },
           dom.button(
             {
               className: "default-button runtime-actions__start " +
                          "js-profile-runtime-button",
               onClick: () => this.onProfilerButtonClick(),
             },
             "Profile Runtime"
           ),
         )
         : null;
  }

  renderTemporaryExtensionInstaller() {
    const { dispatch, runtimeDetails } = this.props;
    const { type } = runtimeDetails.info;
    return isSupportedDebugTargetPane(type, DEBUG_TARGET_PANE.TEMPORARY_EXTENSION)
             ? TemporaryExtensionInstaller({
                 className: "runtime-actions__end",
                 dispatch,
             })
             : null;
  }

  render() {
    return dom.div(
      {
        className: "runtime-actions",
      },
      this.renderConnectionPromptSetting(),
      this.renderProfileButton(),
      this.renderExtensionDebugSetting(),
      this.renderTemporaryExtensionInstaller(),
    );
  }
}

module.exports = RuntimeActions;
