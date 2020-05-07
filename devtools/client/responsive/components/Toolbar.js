/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createFactory,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const DevicePixelRatioMenu = createFactory(
  require("devtools/client/responsive/components/DevicePixelRatioMenu")
);
const DeviceSelector = createFactory(
  require("devtools/client/responsive/components/DeviceSelector")
);
const NetworkThrottlingMenu = createFactory(
  require("devtools/client/shared/components/throttling/NetworkThrottlingMenu")
);
const SettingsMenu = createFactory(
  require("devtools/client/responsive/components/SettingsMenu")
);
const ViewportDimension = createFactory(
  require("devtools/client/responsive/components/ViewportDimension")
);

loader.lazyGetter(this, "UserAgentInput", function() {
  return createFactory(
    require("devtools/client/responsive/components/UserAgentInput")
  );
});

const { getStr } = require("devtools/client/responsive/utils/l10n");
const Types = require("devtools/client/responsive/types");

class Toolbar extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      displayPixelRatio: PropTypes.number.isRequired,
      leftAlignmentEnabled: PropTypes.bool.isRequired,
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      onChangeDevice: PropTypes.func.isRequired,
      onChangeNetworkThrottling: PropTypes.func.isRequired,
      onChangePixelRatio: PropTypes.func.isRequired,
      onChangeTouchSimulation: PropTypes.func.isRequired,
      onChangeUserAgent: PropTypes.func.isRequired,
      onExit: PropTypes.func.isRequired,
      onRemoveDeviceAssociation: PropTypes.func.isRequired,
      doResizeViewport: PropTypes.func.isRequired,
      onRotateViewport: PropTypes.func.isRequired,
      onScreenshot: PropTypes.func.isRequired,
      onToggleLeftAlignment: PropTypes.func.isRequired,
      onToggleReloadOnTouchSimulation: PropTypes.func.isRequired,
      onToggleReloadOnUserAgent: PropTypes.func.isRequired,
      onToggleUserAgentInput: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      selectedDevice: PropTypes.string.isRequired,
      selectedPixelRatio: PropTypes.number.isRequired,
      showUserAgentInput: PropTypes.bool.isRequired,
      touchSimulationEnabled: PropTypes.bool.isRequired,
      viewport: PropTypes.shape(Types.viewport).isRequired,
    };
  }

  renderUserAgent() {
    const { onChangeUserAgent, showUserAgentInput } = this.props;

    if (!showUserAgentInput) {
      return null;
    }

    return createElement(
      Fragment,
      null,
      UserAgentInput({
        onChangeUserAgent,
      }),
      dom.div({ className: "devtools-separator" })
    );
  }

  render() {
    const {
      devices,
      displayPixelRatio,
      leftAlignmentEnabled,
      networkThrottling,
      onChangeDevice,
      onChangeNetworkThrottling,
      onChangePixelRatio,
      onChangeTouchSimulation,
      onExit,
      onRemoveDeviceAssociation,
      doResizeViewport,
      onRotateViewport,
      onScreenshot,
      onToggleLeftAlignment,
      onToggleReloadOnTouchSimulation,
      onToggleReloadOnUserAgent,
      onToggleUserAgentInput,
      onUpdateDeviceModal,
      screenshot,
      selectedDevice,
      selectedPixelRatio,
      showUserAgentInput,
      touchSimulationEnabled,
      viewport,
    } = this.props;

    return dom.header(
      {
        id: "toolbar",
        className: [
          leftAlignmentEnabled ? "left-aligned" : "",
          showUserAgentInput ? "user-agent" : "",
        ]
          .join(" ")
          .trim(),
      },
      dom.div(
        { id: "toolbar-center-controls" },
        DeviceSelector({
          devices,
          onChangeDevice,
          onUpdateDeviceModal,
          selectedDevice,
          viewportId: viewport.id,
        }),
        dom.div({ className: "devtools-separator" }),
        ViewportDimension({
          onRemoveDeviceAssociation,
          doResizeViewport,
          viewport,
        }),
        dom.button({
          id: "rotate-button",
          className: `devtools-button viewport-orientation-${
            viewport.width > viewport.height ? "landscape" : "portrait"
          }`,
          onClick: () => onRotateViewport(viewport.id),
          title: getStr("responsive.rotate"),
        }),
        dom.div({ className: "devtools-separator" }),
        DevicePixelRatioMenu({
          devices,
          displayPixelRatio,
          onChangePixelRatio,
          selectedDevice,
          selectedPixelRatio,
        }),
        dom.div({ className: "devtools-separator" }),
        NetworkThrottlingMenu({
          networkThrottling,
          onChangeNetworkThrottling,
        }),
        dom.div({ className: "devtools-separator" }),
        this.renderUserAgent(),
        dom.button({
          id: "touch-simulation-button",
          className:
            "devtools-button" + (touchSimulationEnabled ? " checked" : ""),
          title: touchSimulationEnabled
            ? getStr("responsive.disableTouch")
            : getStr("responsive.enableTouch"),
          onClick: () => onChangeTouchSimulation(!touchSimulationEnabled),
        })
      ),
      dom.div(
        { id: "toolbar-end-controls" },
        dom.button({
          id: "screenshot-button",
          className: "devtools-button",
          title: getStr("responsive.screenshot"),
          onClick: onScreenshot,
          disabled: screenshot.isCapturing,
        }),
        SettingsMenu({
          onToggleLeftAlignment,
          onToggleReloadOnTouchSimulation,
          onToggleReloadOnUserAgent,
          onToggleUserAgentInput,
        }),
        dom.div({ className: "devtools-separator" }),
        dom.button({
          id: "exit-button",
          className: "devtools-button",
          title: getStr("responsive.exit"),
          onClick: onExit,
        })
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    displayPixelRatio: state.ui.displayPixelRatio,
    leftAlignmentEnabled: state.ui.leftAlignmentEnabled,
    showUserAgentInput: state.ui.showUserAgentInput,
    touchSimulationEnabled: state.ui.touchSimulationEnabled,
  };
};

module.exports = connect(mapStateToProps)(Toolbar);
