/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { PAGES } = require("../../constants");

const DeviceSidebarItemAction = createFactory(require("./DeviceSidebarItemAction"));
const SidebarItem = createFactory(require("./SidebarItem"));
const FIREFOX_ICON = "chrome://devtools/skin/images/aboutdebugging-firefox-logo.svg";
const CONNECT_ICON = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const GLOBE_ICON = "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";

class Sidebar extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      runtimes: PropTypes.array.isRequired,
      selectedPage: PropTypes.string.isRequired,
    };
  }

  renderDevices() {
    const { dispatch, runtimes } = this.props;
    if (!runtimes.networkRuntimes.length) {
      return Localized(
        {
          id: "about-debugging-sidebar-no-devices"
        }, dom.span(
          {
            className: "sidebar__devices__no-devices-message"
          },
          "No devices discovered"
        )
      );
    }

    return runtimes.networkRuntimes.map(runtime => {
      const connectComponent = DeviceSidebarItemAction({
        connected: !!runtime.client,
        dispatch,
        runtimeId: runtime.id,
      });

      return SidebarItem({
        connectComponent,
        id: "networklocation-" + runtime.id,
        dispatch,
        icon: GLOBE_ICON,
        isSelected: false,
        name: runtime.id,
        selectable: false,
      });
    });
  }

  render() {
    const { dispatch, selectedPage } = this.props;

    return dom.aside(
      {
        className: "sidebar",
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
        this.renderDevices()
      )
    );
  }
}

module.exports = Sidebar;
