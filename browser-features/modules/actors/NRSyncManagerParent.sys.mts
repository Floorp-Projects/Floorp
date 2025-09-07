// SPDX-License-Identifier: MPL-2.0




export class NRSyncManagerParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "SyncManager:GetAccountInfo": {
        const { UIState } = ChromeUtils.importESModule(
          "resource://services-sync/UIState.sys.mjs",
        );

        this.sendAsyncMessage(
          "SyncManager:GetAccountInfo",
          JSON.stringify(UIState.get()),
        );
      }
    }
  }
}
