// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
import type { NativeCorpusWindow } from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
export type WorldWindow = NativeCorpusWindow;
export interface WorldMessageManager {
  loadFrameScript(source: string, allFrames: boolean): void;
  addMessageListener(
    name: string,
    listener: (message: { data: { uri: string; json: string | null } }) => void,
  ): void;
  removeMessageListener(
    name: string,
    listener: (message: { data: { uri: string; json: string | null } }) => void,
  ): void;
}
export interface Caught<Value> {
  ok: boolean;
  value?: Value;
  error?: { name: string; message: string };
}
export interface ContentSnapshot {
  phase: number;
  sharedCount: number;
  pageAddedTag: Caught<string | null>;
  deletedByPage: boolean;
  getterCalls: number;
  setterCalls: number;
  throwCalls: number;
  getterReceivers: { isDetail: boolean }[];
  setterReceivers: object[];
  calls: object[];
}
export interface SourceEvent {
  sameOriginalDetail: boolean;
  repeatedGetterIdentity: boolean;
  originalAlias: Caught<boolean>;
  originalCycle: Caught<boolean>;
}
export interface PageEvent {
  kind: string;
  phase: number;
  detailRead: Caught<{ type: string; isNull: boolean }>;
  detailIsNull: boolean;
  repeatedDetailGetterIdentity: Caught<boolean>;
  repeatedEventDetailIdentity: boolean | null;
  token: Caught<string>;
  observedKind: Caught<string>;
  aliases: Caught<{
    aliasIsShared: boolean;
    arrayElementIsShared: boolean;
    selfIsDetail: boolean;
  }>;
  beforePageWrite: Caught<{
    sharedCount: number;
    contentAddedTag: string | null;
    deletedByContent: boolean;
  }>;
  livePriorReference:
    | Caught<{
      priorSharedIsCurrentShared: boolean;
      priorSharedCount: number;
    }>
    | null;
  ownKeys: Caught<string[]>;
  objectPrototype: Caught<{
    hasPageObjectPrototype: boolean;
    constructorIsPageObject: boolean;
  }>;
  accessor: {
    descriptor: Caught<{
      configurable: boolean;
      enumerable: boolean;
      isData: boolean;
      writable: boolean;
      valueIsShared: boolean;
      getterType: string;
      setterType: string;
    }>;
    read: Caught<boolean>;
  };
  pageMutation: {
    shared: Caught<number>;
    added: Caught<boolean>;
    propertyAssignment: Caught<boolean>;
    deleted: Caught<boolean>;
  };
  call: Caught<unknown>;
  explicitReturnedGlobal: Caught<unknown>;
  functionConstructor?: Caught<unknown>;
}
export interface DocumentContract {
  token: string;
  page: {
    token: string;
    events: PageEvent[];
    unexposedGlobals: {
      globalSecretType: string;
      windowSecretType: string;
      chromeRuntimeType: string;
      browserRuntimeType: string;
    };
  };
  content: {
    token: string;
    extensionId: string;
    pageGlobalMarkerType: string;
    payloads: {
      kind: string;
      snapshots: ContentSnapshot[];
      sourceEvents: SourceEvent[];
    }[];
  };
}
export interface WorldReport {
  observationComplete: boolean;
  compatibilityPass: null;
  observation: { top: DocumentContract; frame: DocumentContract };
}
export interface WorldExtensionParent {
  GlobalManager: {
    extensionMap: Map<
      string,
      { id: string; chromeCompatibilityEnabled?: boolean }
    >;
  };
}
