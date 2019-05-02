/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { ICON_LABEL_LEVEL, PAGE_TYPES, RUNTIMES } = require("../../constants");
const Types = require("../../types/index");
loader.lazyRequireGetter(this, "ADB_ADDON_STATES", "devtools/shared/adb/adb-addon", true);

const IconLabel = createFactory(require("../shared/IconLabel"));
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
      isAdbReady: PropTypes.bool.isRequired,
      isScanningUsb: PropTypes.bool.isRequired,
      networkRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      selectedPage: Types.page,
      selectedRuntimeId: PropTypes.string,
      usbRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
    };
  }

  renderAdbStatus() {
    const isUsbEnabled = this.props.isAdbReady &&
      this.props.adbAddonStatus === ADB_ADDON_STATES.INSTALLED;
    const localizationId = isUsbEnabled ? "about-debugging-sidebar-usb-enabled" :
                                          "about-debugging-sidebar-usb-disabled";
    return IconLabel(
      {
          level: isUsbEnabled ? ICON_LABEL_LEVEL.OK : ICON_LABEL_LEVEL.INFO,
      },
      Localized(
        {
          id: localizationId,
        },
        dom.span(
          {
            className: "qa-sidebar-usb-status",
          },
          localizationId
        )
      )
    );
  }

  renderDevicesEmpty() {
    return SidebarItem(
      {},
      Localized(
        {
          id: "about-debugging-sidebar-no-devices",
        },
        dom.aside(
          {
            className: "sidebar__label qa-sidebar-no-devices",
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
      ...this.renderRuntimeItems(GLOBE_ICON, networkRuntimes),
      ...this.renderRuntimeItems(USB_ICON, usbRuntimes),
    ];
  }

  renderRuntimeItems(icon, runtimes) {
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
        isConnecting: runtime.isConnecting,
        isConnectionFailed: runtime.isConnectionFailed,
        isConnectionNotResponding: runtime.isConnectionNotResponding,
        isConnectionTimeout: runtime.isConnectionTimeout,
        isSelected,
        isUnavailable: runtime.isUnavailable,
        isUnplugged: runtime.isUnplugged,
        name,
        runtimeId: runtime.id,
      });
    });
  }

  renderFooter() {
    const HELP_ICON_SRC = "chrome://global/skin/icons/help.svg";
    const SUPPORT_URL = "https://developer.mozilla.org/docs/Tools/about:debugging";

    return dom.footer(
      {
        className: "sidebar__footer",
      },
      dom.ul(
        {},
        SidebarItem(
          {
            className: "sidebar-item--condensed",
            to: SUPPORT_URL,
          },
          dom.span(
            {
              className: "sidebar__footer__support-help",
            },
            Localized(
              {
                id: "about-debugging-sidebar-support-icon",
                attrs: {
                  alt: true,
                },
              },
              dom.img(
                {
                  className: "sidebar__footer__icon",
                  src: HELP_ICON_SRC,
                }
              ),
            ),
            Localized(
              {
                id: "about-debugging-sidebar-support",
              },
              dom.span({}, "about-debugging-sidebar-support"),
            )
          )
        ),
      )
    );
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
            className: "sidebar__adb-status",
          },
          dom.hr({ className: "separator separator--breathe" }),
          this.renderAdbStatus(),
        ),
        this.renderDevices(),
        SidebarItem(
          {
            className: "sidebar-item--breathe sidebar__refresh-usb",
            key: "refresh-devices",
          },
          RefreshDevicesButton({
            dispatch,
            isScanning: isScanningUsb,
          })
        ),
      ),
      this.renderFooter(),
    );
  }
}

module.exports = Sidebar;
