/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRSyncManagerParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "SyncManager:GetAccountInfo": {
        const { UIState } = ChromeUtils.importESModule(
          "resource://services-sync/UIState.sys.mjs",
        );

        // Ensure UIState is initialized before getting account info
        // to avoid returning incomplete data (e.g., missing displayName)
        if (!UIState.isReady()) {
          await UIState.refresh();
        }

        this.sendAsyncMessage(
          "SyncManager:GetAccountInfo",
          JSON.stringify(UIState.get()),
        );
      }
    }
  }
}
