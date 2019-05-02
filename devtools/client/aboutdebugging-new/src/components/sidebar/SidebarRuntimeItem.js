/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Message = createFactory(require("../shared/Message"));
const SidebarItem = createFactory(require("./SidebarItem"));
const Actions = require("../../actions/index");
const { MESSAGE_LEVEL } = require("../../constants");

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
      isConnecting: PropTypes.bool.isRequired,
      isConnectionFailed: PropTypes.bool.isRequired,
      isConnectionNotResponding: PropTypes.bool.isRequired,
      isConnectionTimeout: PropTypes.bool.isRequired,
      isSelected: PropTypes.bool.isRequired,
      isUnavailable: PropTypes.bool.isRequired,
      isUnplugged: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
      runtimeId: PropTypes.string.isRequired,
    };
  }

  renderConnectButton() {
    const { isConnecting } = this.props;
    const localizationId = isConnecting
                             ? "about-debugging-sidebar-item-connect-button-connecting"
                             : "about-debugging-sidebar-item-connect-button";
    return Localized(
      {
        id: localizationId,
      },
      dom.button(
        {
          className: "default-button default-button--micro qa-connect-button",
          disabled: isConnecting,
          onClick: () => {
            const { dispatch, runtimeId } = this.props;
            dispatch(Actions.connectRuntime(runtimeId));
          },
        },
        localizationId
      )
    );
  }

  renderMessage(flag, level, localizationId, className) {
    if (!flag) {
      return null;
    }

    return Message(
      {
        level,
        className: `${className} sidebar-runtime-item__message`,
        isCloseable: true,
      },
      Localized(
        {
          id: localizationId,
        },
        dom.p({ className: "word-wrap-anywhere" }, localizationId)
      )
    );
  }

  renderName() {
    const { deviceName, getString, isUnavailable, isUnplugged, name } = this.props;

    let displayName, qaClassName;
    if (isUnplugged) {
      displayName = getString("about-debugging-sidebar-runtime-item-unplugged");
      qaClassName = "qa-runtime-item-unplugged";
    } else if (isUnavailable) {
      displayName = getString("about-debugging-sidebar-runtime-item-waiting-for-browser");
      qaClassName = "qa-runtime-item-waiting-for-browser";
    } else {
      displayName = name;
      qaClassName = "qa-runtime-item-standard";
    }

    const localizationId = deviceName
      ? "about-debugging-sidebar-runtime-item-name"
      : "about-debugging-sidebar-runtime-item-name-no-device";

    const className = "ellipsis-text sidebar-runtime-item__runtime";

    function renderWithDevice() {
      return dom.span(
        {
          className,
          title: localizationId,
        },
        deviceName,
        dom.br({}),
        dom.span(
          {
            className: `sidebar-runtime-item__runtime__details ${qaClassName}`,
          },
          displayName,
        ),
      );
    }

    function renderNoDevice() {
      return dom.span(
        {
          className,
          title: localizationId,
        },
        displayName,
      );
    }

    return Localized(
      {
        id: localizationId,
        attrs: { title: true },
        $deviceName: deviceName,
        $displayName: displayName,
      },
      deviceName ? renderWithDevice() : renderNoDevice(),
    );
  }

  render() {
    const {
      getString,
      icon,
      isConnected,
      isConnectionFailed,
      isConnectionTimeout,
      isConnectionNotResponding,
      isSelected,
      isUnavailable,
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
      dom.section(
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
        !isUnavailable && !isConnected ? this.renderConnectButton() : null
      ),
      this.renderMessage(
        isConnectionFailed,
        MESSAGE_LEVEL.ERROR,
        "about-debugging-sidebar-item-connect-button-connection-failed",
        "qa-connection-error"
      ),
      this.renderMessage(
        isConnectionTimeout,
        MESSAGE_LEVEL.ERROR,
        "about-debugging-sidebar-item-connect-button-connection-timeout",
        "qa-connection-timeout"
      ),
      this.renderMessage(
        isConnectionNotResponding,
        MESSAGE_LEVEL.WARNING,
        "about-debugging-sidebar-item-connect-button-connection-not-responding",
        "qa-connection-not-responding"
      ),
    );
  }
}

module.exports = FluentReact.withLocalization(SidebarRuntimeItem);
