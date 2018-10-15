/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { PAGES, RUNTIMES } = require("../../constants");
loader.lazyRequireGetter(this, "ADB_ADDON_STATES", "devtools/shared/adb/adb-addon", true);

const DeviceSidebarItemAction = createFactory(require("./DeviceSidebarItemAction"));
const SidebarItem = createFactory(require("./SidebarItem"));
const FIREFOX_ICON = "chrome://devtools/skin/images/aboutdebugging-firefox-logo.svg";
const CONNECT_ICON = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const GLOBE_ICON = "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";
const USB_ICON = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";

class Sidebar extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: PropTypes.string,
      className: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      runtimes: PropTypes.object.isRequired,
      selectedPage: PropTypes.string,
    };
  }

  renderAdbAddonStatus() {
    const isAddonInstalled = this.props.adbAddonStatus === ADB_ADDON_STATES.INSTALLED;
    const localizationId = isAddonInstalled ? "about-debugging-sidebar-usb-enabled" :
                                              "about-debugging-sidebar-usb-disabled";
    return Localized(
      {
        id: localizationId
      }, dom.aside(
        {
          className: "sidebar__devices__message"
        },
        localizationId
      )
    );
  }

  renderDevices() {
    const { runtimes } = this.props;
    if (!runtimes.networkRuntimes.length && !runtimes.usbRuntimes.length) {
      return Localized(
        {
          id: "about-debugging-sidebar-no-devices"
        }, dom.aside(
          {
            className: "sidebar__devices__message js-sidebar-no-devices"
          },
          "No devices discovered"
        )
      );
    }

    return [
      ...this.renderSidebarItems(GLOBE_ICON, runtimes.networkRuntimes),
      ...this.renderSidebarItems(USB_ICON, runtimes.usbRuntimes),
    ];
  }

  renderSidebarItems(icon, runtimes) {
    const { dispatch, selectedPage } = this.props;

    return runtimes.map(runtime => {
      const pageId = runtime.type + "-" + runtime.id;
      const runtimeHasConnection = !!runtime.connection;

      const connectComponent = DeviceSidebarItemAction({
        connected: runtimeHasConnection,
        dispatch,
        runtimeId: runtime.id,
      });

      return SidebarItem({
        connectComponent,
        id: pageId,
        dispatch,
        icon,
        isSelected: selectedPage === pageId,
        key: pageId,
        name: runtime.name,
        runtimeId: runtime.id,
        selectable: runtimeHasConnection,
      });
    });
  }

  render() {
    const { dispatch, selectedPage } = this.props;

    return dom.aside(
      {
        className: `sidebar ${this.props.className || ""}`,
      },
      dom.ul(
        {},
        Localized(
          { id: "about-debugging-sidebar-this-firefox", attrs: { name: true } },
          SidebarItem({
            id: PAGES.THIS_FIREFOX,
            dispatch,
            icon: FIREFOX_ICON,
            isSelected: PAGES.THIS_FIREFOX === selectedPage,
            name: "This Firefox",
            runtimeId: RUNTIMES.THIS_FIREFOX,
            selectable: true,
          })
        ),
        Localized(
          { id: "about-debugging-sidebar-connect", attrs: { name: true } },
          SidebarItem({
            id: PAGES.CONNECT,
            dispatch,
            icon: CONNECT_ICON,
            isSelected: PAGES.CONNECT === selectedPage,
            name: "Connect",
            selectable: true,
          })
        ),
        dom.hr(),
        this.renderAdbAddonStatus(),
        this.renderDevices()
      )
    );
  }
}

module.exports = Sidebar;
