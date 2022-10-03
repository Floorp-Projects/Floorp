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
loader.lazyRequireGetter(
  this,
  "copyString",
  "resource://devtools/shared/platform/clipboard.js",
  true
);
const {
  getMessagePayload,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

class MessageListContextMenu {
  constructor(props) {
    this.props = props;
  }

  /**
   * Handle the context menu opening.
   */
  open(event = {}, item) {
    const menuItems = [
      {
        id: `message-list-context-copy-message`,
        label: L10N.getStr("netmonitor.ws.context.copyFrame"),
        accesskey: L10N.getStr("netmonitor.ws.context.copyFrame.accesskey"),
        click: () => this.copyMessagePayload(item),
      },
    ];

    showMenu(menuItems, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  /**
   * Copy the full payload from the selected message.
   */
  copyMessagePayload(item) {
    getMessagePayload(item.payload, this.props.connector.getLongString).then(
      payload => {
        copyString(payload);
      }
    );
  }
}

module.exports = MessageListContextMenu;
