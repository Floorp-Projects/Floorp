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
      deviceName: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      icon: PropTypes.string.isRequired,
      isConnected: PropTypes.bool.isRequired,
      isSelected: PropTypes.bool.isRequired,
      isUnknown: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
      runtimeId: PropTypes.string.isRequired,
    };
  }

  renderConnectButton() {
    return Localized(
      {
        id: "about-debugging-sidebar-item-connect-button",
      },
      dom.button(
        {
          className: "default-button default-button--micro js-connect-button",
          onClick: () => {
            const { dispatch, runtimeId } = this.props;
            dispatch(Actions.connectRuntime(runtimeId));
          },
        },
        "Connect"
      )
    );
  }

  renderName() {
    const { deviceName, getString, isUnknown, name } = this.props;

    const displayName = isUnknown ?
      getString("about-debugging-sidebar-runtime-item-waiting-for-runtime") : name;

    const titleLocalizationId = deviceName ?
      "about-debugging-sidebar-runtime-item-name" :
      "about-debugging-sidebar-runtime-item-name-no-device";

    return Localized(
      {
        id: titleLocalizationId,
        attrs: { title: true },
        $deviceName: deviceName,
        $displayName: displayName,
      },
      dom.span(
        {
          className: "ellipsis-text",
          title: titleLocalizationId,
        },
        displayName,
        // If a deviceName is available, display it on a separate line.
        ...(deviceName ? [
          dom.br({}),
          deviceName,
        ] : []),
      )
    );
  }

  render() {
    const {
      getString,
      icon,
      isConnected,
      isSelected,
      isUnknown,
      runtimeId,
    } = this.props;

    const connectionStatus = isConnected ?
      getString("aboutdebugging-sidebar-runtime-connection-status-connected") :
      getString("aboutdebugging-sidebar-runtime-connection-status-disconnected");

    return SidebarItem(
      {
        isSelected,
        to: isConnected ? `/runtime/${encodeURIComponent(runtimeId)}` : null,
      },
      dom.div(
        {
          className: "sidebar-runtime-item__container",
        },
        dom.img(
          {
            className: "sidebar-runtime-item__icon ",
            src: icon,
            alt: connectionStatus,
            title: connectionStatus,
          }
        ),
        this.renderName(),
        !isUnknown && !isConnected ? this.renderConnectButton() : null
      )
    );
  }
}

module.exports = FluentReact.withLocalization(SidebarRuntimeItem);
