/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const ConnectionPromptSetting = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/ConnectionPromptSetting.js")
);

const Actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");
const {
  RUNTIMES,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

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
          connectionPromptEnabled,
          dispatch,
        })
      : null;
  }

  renderProfileButton() {
    const { runtimeId } = this.props;

    return runtimeId !== RUNTIMES.THIS_FIREFOX
      ? Localized(
          {
            id: "about-debugging-runtime-profile-button2",
          },
          dom.button(
            {
              className: "default-button qa-profile-runtime-button",
              onClick: () => this.onProfilerButtonClick(),
            },
            "about-debugging-runtime-profile-button2"
          )
        )
      : null;
  }

  render() {
    return dom.div(
      {
        className: "runtime-actions__toolbar",
      },
      this.renderProfileButton(),
      this.renderConnectionPromptSetting()
    );
  }
}

module.exports = RuntimeActions;
