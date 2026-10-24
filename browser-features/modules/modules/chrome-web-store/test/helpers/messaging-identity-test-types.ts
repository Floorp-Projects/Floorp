// SPDX-License-Identifier: MPL-2.0
export type FixtureContext = {
  xulBrowser: {
    contentTitle: string;
    currentURI: nsIURI;
    browsingContext: {
      currentWindowGlobal: { documentPrincipal: nsIPrincipal };
    };
    messageManager: { loadFrameScript(script: string, delayed: boolean): void };
  };
};
export type FixtureExtension = { backgroundContext?: FixtureContext };
export type Fixture = {
  id: string;
  compatible: boolean;
  extension: FixtureExtension;
  addon: { uninstall(): Promise<void> };
  path: string;
};
export type Response = {
  event: string;
  senderId: string;
  senderUrl: string;
  senderOrigin: string;
  ownId: string;
  value: string;
};
export type CommandResult = {
  requestId: number;
  result?: Response & { id: string; url: string; resource: string };
  error?: string;
  errorName?: string;
  synchronousThrow?: boolean;
};
