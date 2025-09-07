// SPDX-License-Identifier: MPL-2.0




export class NRSyncManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRSyncManagerChild created!");
    const window = this.contentWindow;
    if (window?.location.port === "5183") {
      console.debug("NRSyncManager 5183!");
      Cu.exportFunction(this.NRGetAccountInfo.bind(this), window, {
        defineAs: "NRGetAccountInfo",
      });
    }
  }
  NRGetAccountInfo(callback: (accountInfo: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetAccountInfo = resolve;
    });
    this.sendAsyncMessage("SyncManager:GetAccountInfo");
    promise.then((accountInfo) => callback(accountInfo));
  }

  resolveGetAccountInfo: ((accountInfo: string) => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "SyncManager:GetAccountInfo": {
        this.resolveGetAccountInfo?.(message.data);
        this.resolveGetAccountInfo = null;
        break;
      }
    }
  }
}
