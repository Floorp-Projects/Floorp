// SPDX-License-Identifier: MPL-2.0



import { createBirpc } from "birpc";
import { ClientFunctions, ServerFunctions } from "../common/NRTestTypes";

export class NRTestChild extends JSWindowActorChild {
  clientFunctions: ClientFunctions = {};
  rpc = createBirpc<ServerFunctions, ClientFunctions>(this.clientFunctions, {
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
  handleEvent(event: Event) {
    if (event.type === "DOMContentLoaded") {
      this.rpc.onDOMContentLoaded(this.contentWindow?.location.href);
      //this.sendAsyncMessage("DOMContentLoaded", null);
    }
  }
}
