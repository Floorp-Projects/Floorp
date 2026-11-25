// SPDX-License-Identifier: MPL-2.0

import type { HubSnapshot } from "./HubRegistryTypes.ts";

export interface HubPageSnapshotEnvelope {
  type: "snapshot";
  connectionNonce: string;
  requestId: string | null;
  snapshot: HubSnapshot;
}

export interface HubPageAcknowledgement {
  type: "snapshot-ack";
  connectionNonce: string;
  requestId: string;
  epoch: string;
  hubRevision: number;
}
