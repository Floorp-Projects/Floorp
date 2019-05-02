/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("../actions/index");

class ConnectionPromptSetting extends PureComponent {
  static get propTypes() {
    return {
      className: PropTypes.string,
      connectionPromptEnabled: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired,
    };
  }

  onToggleClick() {
    const { connectionPromptEnabled, dispatch } = this.props;
    dispatch(Actions.updateConnectionPromptSetting(!connectionPromptEnabled));
  }

  render() {
    const { className, connectionPromptEnabled } = this.props;

    const localizedState = connectionPromptEnabled
                             ? "about-debugging-connection-prompt-disable-button"
                             : "about-debugging-connection-prompt-enable-button";

    return Localized(
      {
        id: localizedState,
      },
      dom.button(
        {
          className: `${ className } default-button qa-connection-prompt-toggle-button`,
          onClick: () => this.onToggleClick(),
        },
        localizedState
      )
    );
  }
}

module.exports = ConnectionPromptSetting;
