// SPDX-License-Identifier: MPL-2.0

export interface NativeAppService {
  readonly protocolVersion: number;
  readonly capabilitiesJSON: string;
  readonly hostIdentityJSON: string;
  configure(profileId: string): string;
  registerApp(appId: string, bundlePath: string): void;
  launchApp(appId: string): void;
  cancelLaunch(appId: string): void;
  adoptAppBundlePath(appId: string, bundlePath: string): void;
  stop(): void;
  getAppIdForWindow(window: Window): string;
  verifyAppBundle(appId: string, bundlePath: string): string;
  fingerprintBundle(bundlePath: string): string;
  moveStagedBundle(staged: string, backup: string, fingerprint: string): void;
  retireAppBundle(live: string, staged: string, fingerprint: string): void;
  exchangeAppBundles(
    live: string,
    backup: string,
    liveHash: string,
    backupHash: string,
  ): void;
  removeStagedBundle(path: string, fingerprint: string): void;
  withWindowContext(appId: string, factory: { createWindow(): Window }): Window;
  sendControl(appId: string, type: number, payload: string): void;
}

export interface NativeAppWindow extends Window {
  readonly docShell: nsIDocShell;
  BrowserCommands?: { tryToCloseWindow(): void };
  goDoCommand?(command: string): void;
}
