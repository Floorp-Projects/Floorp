// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
import type {
  NativeCorpusExtension,
  NativeCorpusWindow,
} from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";

export interface ScopeExtension extends NativeCorpusExtension {
  backgroundState: string;
  workerBackground: boolean;
  policy: {
    backgroundServiceWorkerAllowed: boolean;
    backgroundTypeModule: boolean;
  };
  terminateBackground(): Promise<void>;
}
export interface ScopeExtensionParent {
  GlobalManager: { extensionMap: Map<string, ScopeExtension> };
}
export type ScopeWindow = NativeCorpusWindow;
export interface Probe {
  starts: number;
  worker: boolean;
  hasDocument: boolean;
  contexts: { contextType: string; contextId: string }[];
}
export interface CommandResult {
  requestId: number;
  result?: Probe;
  error?: string;
}
export interface ScopeObservation {
  label: string;
  declaredScript: string;
  declaredType: string;
  trustedChrome: boolean;
  registration?: { scope: string; scriptSpec: string; workerId: string | null };
  extensionRoot?: string;
  before?: Probe;
  after?: Probe;
  backgroundViewType?: string | null;
  policyAllowed?: boolean;
  policyModule?: boolean;
  stopped?: boolean;
  sameRegistrationAfterRestart?: boolean;
  error?: { name: string; message: string; result: number | null };
  cleanup?: boolean;
}
export interface HTTPObservation {
  root: { scope?: string; error?: string };
  nestedRoot: { scope?: string; error?: string };
  nestedDefault: { scope?: string; error?: string };
  unregistered: boolean[];
}
