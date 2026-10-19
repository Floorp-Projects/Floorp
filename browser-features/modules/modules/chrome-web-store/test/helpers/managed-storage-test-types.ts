// SPDX-License-Identifier: MPL-2.0
import type {
  NativeCorpusExtension,
  NativeCorpusWindow,
} from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";

export interface ManagedStorageObservation {
  name: string;
  value?: unknown;
  error?: string;
}
export interface ManagedStorageResult {
  observations: ManagedStorageObservation[];
  publicId: string;
  worker: boolean;
  hasDocument: boolean;
  module: boolean;
}
export interface ManagedStorageReply {
  requestId: number;
  result?: ManagedStorageResult;
  restricted?: boolean;
  denied?: boolean;
  error?: string;
}
export interface ManagedStorageExtension extends NativeCorpusExtension {
  policy: {
    backgroundServiceWorkerAllowed: boolean;
    backgroundTypeModule: boolean;
  };
}
export interface ManagedStorageParent {
  GlobalManager: { extensionMap: Map<string, ManagedStorageExtension> };
}
export interface ManagedStorageParentImport {
  ExtensionParent: ManagedStorageParent;
}
export interface ManagedStorageAddon {
  uninstall(): Promise<void>;
}
export type ManagedStorageWindow = NativeCorpusWindow;
