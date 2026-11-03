// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export interface UserScriptsPermissionAddon {
  uninstall(): Promise<void>;
}

export interface UserScriptsPermissionAddonManager {
  installTemporaryAddon(file: nsIFile): Promise<UserScriptsPermissionAddon>;
}

export interface UserScriptsPermissionExtension {
  manifest: { permissions: string[]; optional_permissions: string[] };
  permissions: Set<string>;
  hasPermission(name: string): boolean;
  getRequiredPermissions(): { permissions: string[] } | null;
}

export interface UserScriptsPermissionExtensionParent {
  GlobalManager: { extensionMap: Map<string, UserScriptsPermissionExtension> };
}

export interface UserScriptsPermissionCase {
  name: string;
  letter: string;
  metadata: "valid" | "missing" | "mismatched-id";
  required: boolean;
  expectedGranted: boolean;
  manifestVersion: 2 | 3;
}
