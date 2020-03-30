/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");

// Menu
loader.lazyRequireGetter(
  this,
  "showMenu",
  "devtools/client/shared/components/menu/utils",
  true
);

class FrameFilterMenu extends PureComponent {
  static get propTypes() {
    return {
      frameFilterType: PropTypes.string.isRequired,
      toggleFrameFilterType: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowFilterMenu = this.onShowFilterMenu.bind(this);
  }

  onShowFilterMenu(event) {
    const { frameFilterType, toggleFrameFilterType } = this.props;

    const menuItems = [
      {
        id: "ws-frame-list-context-filter-all",
        label: L10N.getStr("netmonitor.ws.context.all"),
        accesskey: L10N.getStr("netmonitor.ws.context.all.accesskey"),
        checked: frameFilterType === "all",
        click: () => {
          toggleFrameFilterType("all");
        },
      },
      {
        id: "ws-frame-list-context-filter-sent",
        label: L10N.getStr("netmonitor.ws.context.sent"),
        accesskey: L10N.getStr("netmonitor.ws.context.sent.accesskey"),
        checked: frameFilterType === "sent",
        click: () => {
          toggleFrameFilterType("sent");
        },
      },
      {
        id: "ws-frame-list-context-filter-received",
        label: L10N.getStr("netmonitor.ws.context.received"),
        accesskey: L10N.getStr("netmonitor.ws.context.received.accesskey"),
        checked: frameFilterType === "received",
        click: () => {
          toggleFrameFilterType("received");
        },
      },
    ];

    showMenu(menuItems, { button: event.target });
  }

  render() {
    const { frameFilterType } = this.props;
    const title = L10N.getStr(`netmonitor.ws.context.${frameFilterType}`);

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

module.exports = FrameFilterMenu;
