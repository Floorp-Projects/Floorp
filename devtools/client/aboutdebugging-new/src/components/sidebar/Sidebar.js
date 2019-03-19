/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { MESSAGE_LEVEL, PAGE_TYPES, RUNTIMES } = require("../../constants");
const Types = require("../../types/index");
loader.lazyRequireGetter(this, "ADB_ADDON_STATES", "devtools/shared/adb/adb-addon", true);

const Message = createFactory(require("../shared/Message"));
const SidebarItem = createFactory(require("./SidebarItem"));
const SidebarFixedItem = createFactory(require("./SidebarFixedItem"));
const SidebarRuntimeItem = createFactory(require("./SidebarRuntimeItem"));
const RefreshDevicesButton = createFactory(require("./RefreshDevicesButton"));
const FIREFOX_ICON = "chrome://devtools/skin/images/aboutdebugging-firefox-logo.svg";
const CONNECT_ICON = "chrome://devtools/skin/images/settings.svg";
const GLOBE_ICON = "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";
const USB_ICON = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";

class Sidebar extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: Types.adbAddonStatus,
      className: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      isScanningUsb: PropTypes.bool.isRequired,
      networkRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      selectedPage: Types.page,
      selectedRuntimeId: PropTypes.string,
      usbRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
    };
  }

  renderAdbAddonStatus() {
    const isAddonInstalled = this.props.adbAddonStatus === ADB_ADDON_STATES.INSTALLED;
    const localizationId = isAddonInstalled ? "about-debugging-sidebar-usb-enabled" :
                                              "about-debugging-sidebar-usb-disabled";
    return Message(
      {
          level: MESSAGE_LEVEL.INFO,
      },
        Localized(
          {
            id: localizationId,
          },
          dom.div(
            {
              className: "js-sidebar-usb-status",
            },
            localizationId
          )
      )
    );
  }

  renderDevicesEmpty() {
    return SidebarItem(
      {
        isSelected: false,
      },
      Localized(
        {
          id: "about-debugging-sidebar-no-devices",
        },
        dom.aside(
          {
            className: "sidebar__label js-sidebar-no-devices",
          },
          "No devices discovered"
        )
      )
    );
  }

  renderDevices() {
    const { networkRuntimes, usbRuntimes } = this.props;

    // render a "no devices" messages when the lists are empty
    if (!networkRuntimes.length && !usbRuntimes.length) {
      return this.renderDevicesEmpty();
    }
    // render all devices otherwise
    return [
      ...this.renderSidebarItems(GLOBE_ICON, networkRuntimes),
      ...this.renderSidebarItems(USB_ICON, usbRuntimes),
    ];
  }

  renderSidebarItems(icon, runtimes) {
    const { dispatch, selectedPage, selectedRuntimeId } = this.props;

    return runtimes.map(runtime => {
      const keyId = `${runtime.type}-${runtime.id}`;
      const runtimeHasDetails = !!runtime.runtimeDetails;
      const isSelected = selectedPage === PAGE_TYPES.RUNTIME &&
        runtime.id === selectedRuntimeId;

      let name = runtime.name;
      if (runtime.type === RUNTIMES.USB && runtimeHasDetails) {
        // Update the name to be same to the runtime page.
        name = runtime.runtimeDetails.info.name;
      }

      return SidebarRuntimeItem({
        deviceName: runtime.extra.deviceName,
        dispatch,
        icon,
        key: keyId,
        isConnected: runtimeHasDetails,
        isSelected,
        isUnknown: runtime.isUnknown,
        name,
        runtimeId: runtime.id,
      });
    });
  }

  render() {
    const { dispatch, selectedPage, selectedRuntimeId, isScanningUsb } = this.props;

    return dom.aside(
      {
        className: `sidebar ${this.props.className || ""}`,
      },
      dom.ul(
        {},
        Localized(
          { id: "about-debugging-sidebar-setup", attrs: { name: true } },
          SidebarFixedItem({
            dispatch,
            icon: CONNECT_ICON,
            isSelected: PAGE_TYPES.CONNECT === selectedPage,
            key: PAGE_TYPES.CONNECT,
            name: "Setup",
            to: "/setup",
          })
        ),
        Localized(
          { id: "about-debugging-sidebar-this-firefox", attrs: { name: true } },
          SidebarFixedItem({
            icon: FIREFOX_ICON,
            isSelected: PAGE_TYPES.RUNTIME === selectedPage &&
              selectedRuntimeId === RUNTIMES.THIS_FIREFOX,
            key: RUNTIMES.THIS_FIREFOX,
            name: "This Firefox",
            to: `/runtime/${RUNTIMES.THIS_FIREFOX}`,
          })
        ),
        SidebarItem(
          {
            className: "sidebar-item--overflow",
            isSelected: false,
          },
          dom.hr({ className: "separator" }),
          this.renderAdbAddonStatus(),
        ),
        this.renderDevices(),
        SidebarItem(
          {
            className: "sidebar-item--breathe sidebar__refresh-usb",
            isSelected: false,
            key: "refresh-devices",
          },
          RefreshDevicesButton({
            dispatch,
            isScanning: isScanningUsb,
          })
        ),
      )
    );
  }
}

module.exports = Sidebar;
