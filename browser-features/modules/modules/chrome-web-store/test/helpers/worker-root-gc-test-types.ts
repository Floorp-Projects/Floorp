// SPDX-License-Identifier: MPL-2.0
export interface WorkerRootGCSnapshot {
  token: string;
  sentinelCollected: boolean;
  rootAlive: boolean;
  runtimeAlive: boolean;
  methodAlive: boolean;
  sameRoot?: boolean;
  sameRuntime?: boolean;
  sameMethod?: boolean;
  authoredValue?: string;
  url?: string;
  storage?: { token: string };
  error?: string;
}

export interface WorkerRootGCResponses {
  arm: { token: string; armed?: true; error?: string };
  probe: WorkerRootGCSnapshot;
}
