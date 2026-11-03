// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export type PowerLockState =
  | "unlocked"
  | "locked-background"
  | "locked-foreground";

export type PowerCommand = "system" | "display" | "release" | "invalid";

export interface ChromePowerManagerService extends nsIPowerManagerService {
  getWakeLockState(topic: string): PowerLockState;
  newWakeLockForChromeExtension(topic: string): nsIWakeLock;
}

export interface PowerBrowser {
  contentTitle: string;
  messageManager: {
    loadFrameScript(source: string, allFrames: boolean): void;
  };
}

export interface PowerTab {
  linkedBrowser: PowerBrowser;
  isConnected: boolean;
}

export interface PowerWindow extends Window {
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): PowerTab;
    removeTab(tab: PowerTab): void;
  };
}

export interface PowerAddon {
  uninstall(): Promise<void>;
}

export interface PowerBackground {
  xulBrowser: PowerBrowser;
}

export interface PowerExtension {
  backgroundContext?: PowerBackground;
  backgroundState: string;
  baseURI: nsIURI;
  principal: nsIPrincipal;
  terminateBackground(): Promise<void>;
}

export interface PowerFixture {
  addon: PowerAddon | null;
  path: string;
  id: string;
  optional: boolean;
  extension: PowerExtension;
  background?: PowerBackground;
}

export interface InitializedPowerFixture extends PowerFixture {
  background: PowerBackground;
}

export interface PowerInstallOptions {
  ordinary?: boolean;
  mismatch?: boolean;
  optional?: boolean;
  grant?: boolean;
  withoutPermission?: boolean;
}

export interface PowerDocumentReport {
  phase: string;
  commandId?: number;
  namespace?: string;
  keys?: string[];
  level?: { SYSTEM: string; DISPLAY: string };
  returns?: string;
  invalid?: boolean;
  validation?: { returned?: boolean; name?: string; typeError?: boolean }[];
}

export interface PowerAddonManagerModule {
  AddonManager: {
    installTemporaryAddon(file: nsIFile): Promise<PowerAddon>;
  };
}

export interface PowerExtensionParentModule {
  ExtensionParent: {
    GlobalManager: { extensionMap: Map<string, PowerExtension> };
  };
}

export interface PowerPermissionSet {
  permissions: string[];
  origins: string[];
}

export interface PowerPermissionsModule {
  ExtensionPermissions: {
    add(id: string, permissions: PowerPermissionSet): Promise<unknown>;
    remove(
      id: string,
      permissions: PowerPermissionSet,
      extension?: PowerExtension,
    ): Promise<unknown>;
  };
}
