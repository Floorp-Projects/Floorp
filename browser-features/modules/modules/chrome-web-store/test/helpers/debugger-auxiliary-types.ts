// SPDX-License-Identifier: MPL-2.0
// Native test-facing handles; production interfaces remain supplied by Gecko.

export type AlertWindow = {
  closed: boolean;
  document: Document;
  arguments: unknown[];
  gIsActive: boolean;
  gRequireInteraction: boolean;
  close(): void;
};

export type NotificationState = {
  ready: boolean;
  sequence?: number;
  value?: unknown;
  error?: string;
  buttons: { id: string; index: number }[];
  closed: { id: string; byUser: boolean }[];
};

export type NativeTab = {
  linkedBrowser: {
    contentTitle: string;
    browsingContext: { currentWindowGlobal: { innerWindowId: number } };
    loadURI(uri: nsIURI, options: { triggeringPrincipal: nsIPrincipal }): void;
  };
};

export type NativeWindow = {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): NativeTab;
    removeTab(tab: NativeTab): void;
    selectedTab: NativeTab;
  };
};

export type FixtureState = {
  ready: boolean;
  sequence?: number;
  value?: unknown;
  error?: string;
};

export type FixtureTab = {
  linkedBrowser: {
    contentTitle: string;
    currentURI: { spec: string };
    browsingContext: Record<string, unknown>;
  };
};

export type FixtureWindow = {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): FixtureTab;
    selectedTab: FixtureTab;
    removeTab(tab: FixtureTab): void;
  };
};

export type TestTab = {
  linkedBrowser: {
    contentTitle: string;
    currentURI: { spec: string };
    browsingContext: {
      currentWindowGlobal: { documentPrincipal: { addonId: string } };
    };
    messageManager: { loadFrameScript(source: string, all: boolean): void };
  };
};

export type TestWindow = {
  gBrowser: {
    tabs: TestTab[];
    selectedTab: TestTab;
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): TestTab;
    removeTab(tab: TestTab, options?: { skipPermitUnload: boolean }): void;
  };
};
