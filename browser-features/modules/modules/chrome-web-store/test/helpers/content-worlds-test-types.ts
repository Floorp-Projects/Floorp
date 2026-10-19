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
export interface WorldCaught<Value> {
  value?: Value;
  error?: { name: string; message: string };
}
export interface WorldReferenceObservation {
  properties: Record<string, {
    has: WorldCaught<boolean>;
    own: WorldCaught<boolean>;
    descriptor: WorldCaught<
      null | {
        configurable: boolean;
        enumerable: boolean;
        accessor: boolean;
        hasGetter: boolean;
        hasSetter: boolean;
      }
    >;
    read: WorldCaught<{ type: string }>;
  }>;
  ownKeys: WorldCaught<{ selected: string[] }>;
}
export interface WorldGlobalObservation {
  publicDomPageMarker: string;
  isTop: boolean;
  references: {
    window: WorldReferenceObservation;
    globalThis: WorldReferenceObservation;
    top: WorldCaught<WorldReferenceObservation>;
    parent: WorldCaught<WorldReferenceObservation>;
    iframe: WorldCaught<WorldReferenceObservation | null>;
  };
}
interface IsolatedObservation extends WorldGlobalObservation {
  extensionId: string;
  publicDomPageMarker: string;
  isTop: boolean;
  privateIsolation: {
    globalWrite: boolean;
    windowWrite: boolean;
    pageOwnMarkerType: string;
  };
}
export interface WorldReport {
  complete: boolean;
  extensionExpected: boolean;
  observation: {
    topPage: WorldGlobalObservation;
    framePage: WorldGlobalObservation;
    topIsolated: IsolatedObservation | null;
    frameIsolated: IsolatedObservation | null;
    privateMarkersVisibleToPage: {
      topGlobal: string;
      topWindow: string;
      frameGlobal: string;
      frameWindow: string;
    };
  };
}
export interface WorldExtensionParent {
  GlobalManager: {
    extensionMap: Map<
      string,
      { id: string; chromeCompatibilityEnabled?: boolean }
    >;
  };
}
