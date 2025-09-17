// SPDX-License-Identifier: MPL-2.0




export class NRTabManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRTabManagerChild created!");
    const window = this.contentWindow;
    if (window?.location.port === "5183") {
      console.debug("NRTabManager 5183!");
      Cu.exportFunction(this.NRAddTab.bind(this), window, {
        defineAs: "NRAddTab",
      });
    }
  }
  NRAddTab(url: string, options: object = {}, callback: () => void = () => {}) {
    const promise = new Promise<void>((resolve, _reject) => {
      this.resolveAddTab = resolve;
    });
    this.sendAsyncMessage("Tabs:AddTab", { url, options });
    promise.then((_v) => callback());
  }

  resolveAddTab: (() => void) | null = null;
  resolveLoadTrustedUrl: (() => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "Tabs:AddTab": {
        this.resolveAddTab?.();
        this.resolveLoadTrustedUrl = null;
        break;
      }
    }
  }
}
