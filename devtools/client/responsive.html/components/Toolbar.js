/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { getStr } = require("../utils/l10n");
const Types = require("../types");
const DevicePixelRatioSelector = createFactory(require("./DevicePixelRatioSelector"));
const DeviceSelector = createFactory(require("./DeviceSelector"));
const NetworkThrottlingSelector = createFactory(require("devtools/client/shared/components/throttling/NetworkThrottlingSelector"));
const ReloadConditions = createFactory(require("./ReloadConditions"));

class Toolbar extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      displayPixelRatio: Types.pixelRatio.value.isRequired,
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      reloadConditions: PropTypes.shape(Types.reloadConditions).isRequired,
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      selectedDevice: PropTypes.string.isRequired,
      selectedPixelRatio: PropTypes.shape(Types.pixelRatio).isRequired,
      touchSimulation: PropTypes.shape(Types.touchSimulation).isRequired,
      onChangeDevice: PropTypes.func.isRequired,
      onChangeNetworkThrottling: PropTypes.func.isRequired,
      onChangePixelRatio: PropTypes.func.isRequired,
      onChangeReloadCondition: PropTypes.func.isRequired,
      onChangeTouchSimulation: PropTypes.func.isRequired,
      onExit: PropTypes.func.isRequired,
      onResizeViewport: PropTypes.func.isRequired,
      onScreenshot: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      devices,
      displayPixelRatio,
      networkThrottling,
      reloadConditions,
      screenshot,
      selectedDevice,
      selectedPixelRatio,
      touchSimulation,
      onChangeDevice,
      onChangeNetworkThrottling,
      onChangePixelRatio,
      onChangeReloadCondition,
      onChangeTouchSimulation,
      onExit,
      onResizeViewport,
      onScreenshot,
      onUpdateDeviceModal,
    } = this.props;

    let touchButtonClass = "toolbar-button devtools-button";
    if (touchSimulation.enabled) {
      touchButtonClass += " checked";
    }

    return dom.header(
      { id: "toolbar" },
      DeviceSelector({
        devices,
        selectedDevice,
        viewportId: 0,
        onChangeDevice,
        onResizeViewport,
        onUpdateDeviceModal,
      }),
      dom.div(
        { id: "toolbar-center-controls" },
        DevicePixelRatioSelector({
          devices,
          displayPixelRatio,
          selectedDevice,
          selectedPixelRatio,
          onChangePixelRatio,
        }),
        NetworkThrottlingSelector({
          networkThrottling,
          onChangeNetworkThrottling,
        }),
        dom.button({
          id: "touch-simulation-button",
          className: touchButtonClass,
          title: (touchSimulation.enabled ?
            getStr("responsive.disableTouch") : getStr("responsive.enableTouch")),
          onClick: () => onChangeTouchSimulation(!touchSimulation.enabled),
        })
      ),
      dom.div(
        { id: "toolbar-end-controls" },
        dom.button({
          id: "screenshot-button",
          className: "toolbar-button devtools-button",
          title: getStr("responsive.screenshot"),
          onClick: onScreenshot,
          disabled: screenshot.isCapturing,
        }),
        ReloadConditions({
          reloadConditions,
          onChangeReloadCondition,
        }),
        dom.button({
          id: "exit-button",
          className: "toolbar-button devtools-button",
          title: getStr("responsive.exit"),
          onClick: onExit,
        })
      )
    );
  }
}

module.exports = Toolbar;
