// SPDX-License-Identifier: MPL-2.0



import { createBirpc } from "birpc";
import { ClientFunctions, ServerFunctions } from "../common/NRTestTypes";

export class NRTestParent extends JSWindowActorParent {
  serverFunctions: ServerFunctions = {
    onDOMContentLoaded: function (href: string | undefined): void {
      Services.obs.notifyObservers(
        { type: "DOMContentLoaded", href } as nsISupports,
        "NRTest",
      );
    },
  };
  rpc = createBirpc<ServerFunctions, ClientFunctions>(this.serverFunctions, {
    post: (data) => this.sendAsyncMessage("rpc", data),
    on: (fn) => (this.onRPCMessage = fn),
    serialize: (v) => JSON.stringify(v),
    deserialize: (v) => JSON.parse(v),
  });
  onRPCMessage: ((data: unknown) => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    if (message.name === "rpc") {
      this.onRPCMessage?.(message.data);
    }
  }
}
