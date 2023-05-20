/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");

loader.lazyRequireGetter(
  this,
  "showMenu",
  "resource://devtools/client/shared/components/menu/utils.js",
  true
);

class RequestBlockingContextMenu {
  constructor(props) {
    this.props = props;
  }

  createMenu(contextMenuOptions) {
    const {
      removeAllBlockedUrls,
      disableAllBlockedUrls,
      enableAllBlockedUrls,
    } = this.props;

    const { disableDisableAllBlockedUrls, disableEnableAllBlockedUrls } =
      contextMenuOptions;

    const menu = [
      {
        id: "request-blocking-enable-all",
        label: L10N.getStr(
          "netmonitor.requestBlockingMenu.enableAllBlockedUrls"
        ),
        accesskey: "",
        disabled: disableEnableAllBlockedUrls,
        visible: true,
        click: () => enableAllBlockedUrls(),
      },
      {
        id: "request-blocking-disable-all",
        label: L10N.getStr(
          "netmonitor.requestBlockingMenu.disableAllBlockedUrls"
        ),
        accesskey: "",
        disabled: disableDisableAllBlockedUrls,
        visible: true,
        click: () => disableAllBlockedUrls(),
      },
      {
        id: "request-blocking-remove-all",
        label: L10N.getStr(
          "netmonitor.requestBlockingMenu.removeAllBlockedUrls"
        ),
        accesskey: "",
        visible: true,
        click: () => removeAllBlockedUrls(),
      },
    ];

    return menu;
  }

  open(event, contextMenuOptions) {
    const menu = this.createMenu(contextMenuOptions);

    showMenu(menu, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }
}

module.exports = RequestBlockingContextMenu;
