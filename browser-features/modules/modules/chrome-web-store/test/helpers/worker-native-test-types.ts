// SPDX-License-Identifier: MPL-2.0
import type { NativeCorpusTab } from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";

export interface WorkerNativeWindow extends Window {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): NativeCorpusTab;
    removeTab(tab: NativeCorpusTab): void;
  };
}

export interface WorkerRuntimeContext {
  contextId: string;
  contextType: string;
  frameId: number;
  tabId: number;
  windowId: number;
  incognito: boolean;
  documentId?: string;
  documentOrigin?: string;
  documentUrl?: string;
}
