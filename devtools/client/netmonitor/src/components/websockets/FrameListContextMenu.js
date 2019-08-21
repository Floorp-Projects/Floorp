/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
loader.lazyRequireGetter(
  this,
  "showMenu",
  "devtools/client/shared/components/menu/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "copyString",
  "devtools/shared/platform/clipboard",
  true
);
const {
  getFramePayload,
} = require("devtools/client/netmonitor/src/utils/request-utils.js");

class FrameListContextMenu {
  constructor(props) {
    this.props = props;
  }

  /**
   * Handle the context menu opening.
   */
  open(event = {}, item) {
    const menuItems = [
      {
        id: `ws-frames-list-context-copy-frame`,
        label: L10N.getStr("netmonitor.ws.context.copyFrame"),
        accesskey: L10N.getStr("netmonitor.ws.context.copyFrame.accesskey"),
        click: () => this.copyFramePayload(item),
      },
    ];

    showMenu(menuItems, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  /**
   * Copy the full payload from the selected WS frame.
   */
  copyFramePayload(item) {
    getFramePayload(item.payload, this.props.connector.getLongString).then(
      payload => {
        copyString(payload);
      }
    );
  }
}

module.exports = FrameListContextMenu;
