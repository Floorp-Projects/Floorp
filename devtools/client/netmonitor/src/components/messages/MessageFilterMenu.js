/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");

// Menu
loader.lazyRequireGetter(
  this,
  "showMenu",
  "resource://devtools/client/shared/components/menu/utils.js",
  true
);

class MessageFilterMenu extends PureComponent {
  static get propTypes() {
    return {
      messageFilterType: PropTypes.string.isRequired,
      toggleMessageFilterType: PropTypes.func.isRequired,
      // showControlFrames decides if control frames
      // will be shown in messages panel
      showControlFrames: PropTypes.bool.isRequired,
      // toggleControlFrames toggles the value for showControlFrames
      toggleControlFrames: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowFilterMenu = this.onShowFilterMenu.bind(this);
  }

  onShowFilterMenu(event) {
    const {
      messageFilterType,
      toggleMessageFilterType,
      showControlFrames,
      toggleControlFrames,
    } = this.props;

    const menuItems = [
      {
        id: "message-list-context-filter-all",
        label: L10N.getStr("netmonitor.ws.context.all"),
        accesskey: L10N.getStr("netmonitor.ws.context.all.accesskey"),
        type: "checkbox",
        checked: messageFilterType === "all",
        click: () => {
          toggleMessageFilterType("all");
        },
      },
      {
        id: "message-list-context-filter-sent",
        label: L10N.getStr("netmonitor.ws.context.sent"),
        accesskey: L10N.getStr("netmonitor.ws.context.sent.accesskey"),
        type: "checkbox",
        checked: messageFilterType === "sent",
        click: () => {
          toggleMessageFilterType("sent");
        },
      },
      {
        id: "message-list-context-filter-received",
        label: L10N.getStr("netmonitor.ws.context.received"),
        accesskey: L10N.getStr("netmonitor.ws.context.received.accesskey"),
        type: "checkbox",
        checked: messageFilterType === "received",
        click: () => {
          toggleMessageFilterType("received");
        },
      },
      {
        type: "separator",
      },
      {
        id: "message-list-context-filter-controlFrames",
        label: L10N.getStr("netmonitor.ws.context.controlFrames"),
        accesskey: L10N.getStr("netmonitor.ws.context.controlFrames.accesskey"),
        type: "checkbox",
        checked: showControlFrames,
        click: () => {
          toggleControlFrames();
        },
      },
    ];

    showMenu(menuItems, { button: event.target });
  }

  render() {
    const { messageFilterType, showControlFrames } = this.props;
    const messageFilterTypeTitle = L10N.getStr(
      `netmonitor.ws.context.${messageFilterType}`
    );
    const title =
      messageFilterTypeTitle +
      (showControlFrames
        ? " (" + L10N.getStr(`netmonitor.ws.context.controlFrames`) + ")"
        : "");

    return dom.button(
      {
        id: "frame-filter-menu",
        className: "devtools-button devtools-dropdown-button",
        title,
        onClick: this.onShowFilterMenu,
      },
      dom.span({ className: "title" }, title)
    );
  }
}

module.exports = MessageFilterMenu;
