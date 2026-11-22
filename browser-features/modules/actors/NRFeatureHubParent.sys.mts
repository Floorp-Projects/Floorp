// SPDX-License-Identifier: MPL-2.0

import { HubPageDiscoveryService } from "../modules/feature-host/HubPageDiscovery.sys.mts";

export class NRFeatureHubParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument): void {
    const manager = this.manager;
    if (!manager) return;
    if (message.name === "NRFeatureHub:Connect") {
      HubPageDiscoveryService.connectActor(
        this,
        manager,
        message.data,
      );
    } else if (message.name === "NRFeatureHub:Disconnect") {
      HubPageDiscoveryService.disconnectRequest(this, message.data);
    } else if (message.name === "NRFeatureHub:Acknowledge") {
      HubPageDiscoveryService.handleAcknowledgement(this, message.data);
    }
  }

  didDestroy(): void {
    HubPageDiscoveryService.closeActor(this);
  }
}
