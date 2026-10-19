// SPDX-License-Identifier: MPL-2.0
import type {
  NativeCorpusExtension,
  NativeCorpusWindow,
} from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";

export type WorkerExtension = NativeCorpusExtension & {
  backgroundState: string;
  terminateBackground(): Promise<void>;
};
export type Shape = {
  generation: string;
  sameChrome: boolean;
  isWorker: boolean;
  hasDocument: boolean;
  phase: string;
  contains: boolean;
  power: string;
  tabCapture: string;
  keys: string[];
  powerIn: boolean;
  captureIn: boolean;
  samePower: boolean | null;
  sameCapture: boolean | null;
  sameMethods: boolean | null;
  events: { event: string; permissions: string[] }[];
  level?: { SYSTEM: string; DISPLAY: string };
  captureState?: { ACTIVE: string };
  error?: string;
};
export type OrdinaryObject = {
  plainPrototype: boolean;
  tag: string;
  getterValue: string;
  getterReceiver: boolean;
  lockedValue: number;
  lockedFlags: boolean;
  setLocked: boolean;
  deleteLocked: boolean;
  normalValue: number;
  deleted: boolean;
  unknownType: string;
  thenType: string;
  constructorIsObject: boolean;
};
export type Denial = { synchronous: boolean; name?: string; message?: string };
export type MethodShape = {
  type: string;
  inNamespace: boolean;
  own: boolean;
  keys: boolean;
  descriptor: {
    writable: boolean;
    enumerable: boolean;
    configurable: boolean;
    valueType: string;
    getType: string;
    setType: string;
  } | null;
};
export type ManagementPermission = {
  namespaceType: string;
  selfId: string;
  foreignSelfId: string;
  detachedURL: string;
  foreignURL: string;
  boundURL: string;
  getSelf: MethodShape;
  getURL: MethodShape;
  privileged: Record<string, MethodShape>;
};
export type StorageMirror = {
  wasmType: string;
  sharedPrototype: boolean;
  calls: { script: number; getter: number; proxy: number };
  areas: Record<"local" | "session" | "sync", {
    value: string;
    get: MethodShape;
    set: MethodShape;
    poisonOwnKeys: string[];
  }>;
};
export type PromptWindow = NativeCorpusWindow & {
  PopupNotifications: {
    getNotification(id: string): { options: { name?: string } } | null;
  };
};
