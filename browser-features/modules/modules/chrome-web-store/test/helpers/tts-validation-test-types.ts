// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
export interface TtsValidationBrowser {
  contentTitle: string;
  messageManager: { loadFrameScript(source: string, delayed: boolean): void };
}
export interface TtsValidationTab {
  linkedBrowser: TtsValidationBrowser;
  isConnected: boolean;
}
export interface TtsValidationWindow extends Window {
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): TtsValidationTab;
    removeTab(tab: TtsValidationTab): void;
  };
}
export interface TtsValidationExtension {
  baseURI: nsIURI;
  backgroundState: string;
  backgroundContext: { viewType: string } | null;
}
export interface TtsValidationParent {
  GlobalManager: { extensionMap: Map<string, TtsValidationExtension> };
}
export interface TtsValidationResult {
  context: string;
  isWorker: boolean;
  before: boolean;
  after: boolean;
  eventCount: number;
  invalid: { field: string; value: string; name: string; message: string }[];
}
export interface TtsValidationReply {
  requestId: string;
  result?: TtsValidationResult;
  namespace?: string;
  error?: string;
}
