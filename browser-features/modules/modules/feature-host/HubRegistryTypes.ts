// SPDX-License-Identifier: MPL-2.0

export type HubScalar = string | number | boolean;

export interface HubNavigationDescriptor {
  labelKey: string;
  order?: number;
}

export interface HubMessageViewDescriptor {
  kind: "message";
  titleKey: string;
  bodyKey: string;
  params?: Readonly<Record<string, HubScalar>>;
}

export interface HubPageDescriptor {
  localId: string;
  route: string;
  titleKey: string;
  navigation?: HubNavigationDescriptor;
  view: HubMessageViewDescriptor;
}

export interface HubOwner {
  featureId: string;
  hostEpoch: string;
  generation: number;
}

export interface HubSnapshotEntry extends HubPageDescriptor {
  featureId: string;
  ownerHostEpoch: string;
  ownerGeneration: number;
}

export interface HubSnapshot {
  epoch: string;
  hubRevision: number;
  entries: readonly HubSnapshotEntry[];
}

export interface HubSnapshotMessage {
  type: "snapshot";
  snapshot: HubSnapshot;
}

export interface HubSnapshotAcknowledgement {
  type: "snapshot-ack";
  epoch: string;
  hubRevision: number;
}

export interface HubClientConnection {
  readonly clientId: string;
  readonly connectionId: string;
  readonly windowId: string;
  isClosed(): boolean;
  sendSnapshot(json: string): void;
  onClosed(callback: () => void): () => void;
}

export interface HubLiveDocument {
  readonly documentId: string;
  readonly windowId: string;
  isLive(): boolean;
  ensureParentClient(): HubClientConnection | null;
  onParentClientAvailable(callback: () => void): () => void;
  onClosed(callback: () => void): () => void;
}

export interface HubClientDiscoveryAdapter {
  discoverLiveHubDocuments(
    windowIds: readonly string[],
  ): readonly HubLiveDocument[];
}

export interface HubHostClaim {
  readonly featureId: string;
  readonly hostEpoch: string;
  release(): void;
}

export interface HubGenerationLease {
  readonly owner: HubOwner;
  registerDescriptor(json: string): void;
  dispose(): void;
}

export interface HubRegistryDiagnosticsForTests {
  hubRevision: number;
  stagedEntries: number;
  publishedEntries: number;
  claims: number;
  leases: number;
  clients: number;
  pendingAcknowledgements: number;
  pendingTimers: number;
}
