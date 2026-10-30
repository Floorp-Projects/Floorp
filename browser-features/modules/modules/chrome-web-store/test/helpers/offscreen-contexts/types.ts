// SPDX-License-Identifier: MPL-2.0
export interface ContextRecord {
  contextId: string;
  contextType: string;
  documentId?: string;
  documentUrl?: string;
  documentOrigin?: string;
}
export interface OffscreenResult {
  runtimeId: string;
  worker: boolean;
  module: boolean;
  requestedURL: string;
  urlOrigin: string;
  identity: { runtimeId: string; href: string; origin: string; nonce: string };
  unfiltered: ContextRecord[];
  filtered: ContextRecord[];
  byId: ContextRecord[];
  byDocumentId: ContextRecord[];
  byReturnedOrigin: ContextRecord[];
  foreign: ContextRecord[];
  remaining?: ContextRecord[];
  immediateRemaining?: ContextRecord[];
  elapsed?: number;
}
export interface NativeOffscreenContext {
  extension: { id: string };
  viewType: string;
  toExtensionContext(): ContextRecord | undefined;
  browsingContext?: {
    currentWindowContext?: {
      documentURI: { spec: string };
      documentPrincipal: {
        originNoSuffix: string;
        addonPolicy?: { id: string };
      };
    };
  };
}
export interface OffscreenParent {
  ParentAPIManager: { proxyContexts: Map<unknown, NativeOffscreenContext> };
}
