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

const toHexString = str =>
  Array.from(str, c => c.charCodeAt(0).toString(16).padStart(2, "0")).join("");

class MessageListContextMenu {
  constructor(props) {
    this.props = props;
  }

  /**
   * Handle the context menu opening.
   */
  open(event = {}, item) {
    const menuItems = [];
    if (this.props.showBinaryOptions) {
      menuItems.push(
        {
          id: "message-list-context-copy-message-base64",
          label: L10N.getStr("netmonitor.ws.context.copyFrameAsBase64"),
          accesskey: L10N.getStr(
            "netmonitor.ws.context.copyFrameAsBase64.accesskey"
          ),
          click: () => this.copyMessagePayload(item, btoa),
        },
        {
          id: "message-list-context-copy-message-hex",
          label: L10N.getStr("netmonitor.ws.context.copyFrameAsHex"),
          accesskey: L10N.getStr(
            "netmonitor.ws.context.copyFrameAsHex.accesskey"
          ),
          click: () => this.copyMessagePayload(item, toHexString),
        },
        {
          id: "message-list-context-copy-message-text",
          label: L10N.getStr("netmonitor.ws.context.copyFrameAsText"),
          accesskey: L10N.getStr(
            "netmonitor.ws.context.copyFrameAsText.accesskey"
          ),
          click: () => this.copyMessagePayload(item),
        }
      );
    } else {
      menuItems.push({
        id: "message-list-context-copy-message",
        label: L10N.getStr("netmonitor.ws.context.copyFrame"),
        accesskey: L10N.getStr("netmonitor.ws.context.copyFrame.accesskey"),
        click: () => this.copyMessagePayload(item),
      });
    }

    showMenu(menuItems, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  /**
   * Copy the full payload from the selected message.
   * Can optionally format the payload before copying.
   *
   * @param {object} item
   * @param {object|string} item.payload
   *   Payload to copy.
   * @param {function(string): string} [format]
   *   Optional function to format the payload before copying.
   */
  copyMessagePayload(item, format) {
    getMessagePayload(item.payload, this.props.connector.getLongString).then(
      payload => {
        if (format) {
          payload = format(payload);
        }
        copyString(payload);
      }
    );
  }
}

module.exports = MessageListContextMenu;
