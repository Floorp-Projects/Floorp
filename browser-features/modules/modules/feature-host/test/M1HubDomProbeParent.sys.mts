// SPDX-License-Identifier: MPL-2.0

import { HubPageDiscoveryService } from "../HubPageDiscovery.sys.mts";

export class M1HubDomProbeParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument): string {
    if (
      message.name !== "M1HubDomProbe:FaultInjectTransportDisconnect" ||
      message.data !== undefined
    ) return JSON.stringify({ ok: false });
    const manager = this.manager;
    if (!manager) return JSON.stringify({ ok: false });
    let actor: JSWindowActorParent | null;
    try {
      actor = manager.getActor("NRFeatureHub");
    } catch {
      return JSON.stringify({ ok: false });
    }
    if (!actor) return JSON.stringify({ ok: false });
    HubPageDiscoveryService.disconnectActor(
      actor as Parameters<typeof HubPageDiscoveryService.disconnectActor>[0],
    );
    this.sendAsyncMessage("M1HubDomProbe:EmitDisconnected");
    return JSON.stringify({ ok: true });
  }
}
