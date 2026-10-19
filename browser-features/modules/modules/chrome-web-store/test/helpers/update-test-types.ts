// SPDX-License-Identifier: MPL-2.0
export interface NativeAddon {
  id: string;
  version: string;
  type: string;
  getResourceURI(path: string): { spec: string };
  temporarilyInstalled: boolean;
  isActive: boolean;
  disable(): Promise<void>;
  enable(): Promise<void>;
  userPermissions: { permissions: string[]; origins: string[] };
  uninstall(): Promise<void>;
}

export interface NativeInstall {
  addon?: NativeAddon;
  error: number;
  state: number;
  addListener(listener: object): void;
  removeListener(listener: object): void;
  cancel(): void;
  continuePostponedInstall(): void;
}

export interface NativeManager {
  getInstallForFile(file: nsIFile): Promise<NativeInstall>;
  getAddonByID(id: string): Promise<NativeAddon | null>;
  getAllInstalls(): Promise<NativeInstall[]>;
  installAddonFromAOMWithOptions(
    browser: Element,
    uri: null,
    install: NativeInstall,
    options: { preferUpdateOverInstall: boolean },
  ): void;
  STATE_POSTPONED: number;
  shouldAutoUpdate(addon: NativeAddon): boolean;
  addAddonListener(listener: object): void;
  removeAddonListener(listener: object): void;
}

export interface Background {
  viewType: string;
  unloaded: boolean;
  xulBrowser?: object;
}

export interface NativeExtension {
  baseURI: nsIURI;
  broadcast(
    name: "Extension:FlushJarCache",
    data: { path: string },
  ): Promise<void>;
  backgroundContext?: Background;
  views: Set<Background>;
}

export interface UpdateWorkerSnapshot {
  extension: NativeExtension;
  context: Background;
  contextId: string;
}

export interface Reply {
  ready?: boolean;
  request?: number;
  value?: string;
  id?: string;
  version?: string;
  pending?: string;
  contextId?: string;
  noDocument?: boolean;
  noWindow?: boolean;
  error?: string;
}

export interface PermissionPrompt {
  addon: { id: string; version: string };
  install: NativeInstall;
  resolve(): void;
  reject(): void;
}
