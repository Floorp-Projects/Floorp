// SPDX-License-Identifier: MPL-2.0
export type Background = {
  contentTitle: string;
  messageManager: { loadFrameScript(script: string, delayed: boolean): void };
};
export type Extension = { backgroundContext?: { xulBrowser: Background } };
export type Fixture = { id: string; extension: Extension };
export type Reply = { requestId: number; result?: unknown; error?: string };

export interface AuthWindow extends Window {
  readonly document: Document & {
    readonly isUncommittedInitialDocument: boolean;
  };
}

export type AuthWindowPhase = {
  uri: string;
  readyState: DocumentReadyState;
  initial: boolean;
};
