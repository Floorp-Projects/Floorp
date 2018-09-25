/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("../../actions/index");

/**
 * This component displays actions for sidebar items representing a device.
 */
class DeviceSidebarItemAction extends PureComponent {
  static get propTypes() {
    return {
      connected: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired,
      runtimeId: PropTypes.string.isRequired,
    };
  }

  render() {
    const { connected } = this.props;
    if (connected) {
      return Localized(
        {
          id: "about-debugging-sidebar-item-connected-label"
        },
        dom.span({}, "Connected")
      );
    }

    return Localized(
      {
        id: "about-debugging-sidebar-item-connect-button"
      },
      dom.button(
        {
          className: "sidebar-item__connect-button",
          onClick: () => {
            const { dispatch, runtimeId } = this.props;
            dispatch(Actions.connectRuntime(runtimeId));
          }
        },
        "Connect"
      )
    );
  }
}

module.exports = DeviceSidebarItemAction;
