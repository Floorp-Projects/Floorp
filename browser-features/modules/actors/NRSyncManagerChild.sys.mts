/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRSyncManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRSyncManagerChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRSyncManager 5183 ! or Chrome Page!");
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
