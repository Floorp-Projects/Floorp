// SPDX-License-Identifier: MPL-2.0




export class NRAppConstantsChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRAppConstantsChild created!");
    const window = this.contentWindow;
    if (window?.location.port === "5183") {
      console.debug("NRAppConstants 5183!");
      Cu.exportFunction(this.NRGetConstants.bind(this), window, {
        defineAs: "NRGetConstants",
      });
    }
  }

  NRGetConstants(callback: (constants: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetConstants = resolve;
    });
    this.sendAsyncMessage("AppConstants:GetConstants");
    promise.then((constants) => callback(constants));
  }

  resolveGetConstants: ((constants: string) => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "AppConstants:GetConstants": {
        this.resolveGetConstants?.(message.data);
        this.resolveGetConstants = null;
        break;
      }
    }
  }
}
