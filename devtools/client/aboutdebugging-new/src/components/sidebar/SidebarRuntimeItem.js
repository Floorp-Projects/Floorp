/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const SidebarItem = createFactory(require("./SidebarItem"));
const Actions = require("../../actions/index");

/**
 * This component displays a runtime item of the Sidebar component.
 */
class SidebarRuntimeItem extends PureComponent {
  static get propTypes() {
    return {
      id: PropTypes.string.isRequired,
      deviceName: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      icon: PropTypes.string.isRequired,
      isConnected: PropTypes.bool.isRequired,
      isSelected: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
      runtimeId: PropTypes.string.isRequired,
    };
  }

  renderConnectButton() {
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

  renderNameWithDevice(name, device) {
    return dom.span(
      {
        className: "ellipsis-text",
        title: `${name} (${device})`
      },
      `${name}`,
      dom.br({}),
      device
    );
  }

  renderName(name) {
    return dom.span(
      {
        className: "ellipsis-text",
        title: name
      },
      `${name}`
    );
  }

  render() {
    const {
      deviceName,
      dispatch,
      getString,
      icon,
      id,
      isConnected,
      isSelected,
      name,
      runtimeId,
    } = this.props;

    const connectionStatus = isConnected ?
      getString("aboutdebugging-sidebar-runtime-connection-status-connected") :
      getString("aboutdebugging-sidebar-runtime-connection-status-disconnected");

    return SidebarItem(
      {
        isSelected,
        selectable: isConnected,
        className: "sidebar-runtime-item",
        onSelect: () => {
          dispatch(Actions.selectPage(id, runtimeId));
        }
      },
      dom.img(
        {
          className: "sidebar-runtime-item__icon " +
            `${isConnected ? "sidebar-runtime-item__icon--connected" : "" }`,
          src: icon,
          alt: connectionStatus,
          title: connectionStatus
        }
      ),
      deviceName ? this.renderNameWithDevice(name, deviceName) : this.renderName(name),
      !isConnected ? this.renderConnectButton() : null
    );
  }
}

module.exports = FluentReact.withLocalization(SidebarRuntimeItem);
