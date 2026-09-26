// SPDX-License-Identifier: MPL-2.0

/** Capabilities reported by the Runtime, never inferred from an installed binary. */
export interface MacAppShimCapabilities {
  protocolVersion: number;
  authenticatedTransport: boolean;
  nativeWindowOwnership: boolean;
  sharedBrowserProfile: boolean;
  transactionalInstall: boolean;
}

export interface LegacyAppRegistration {
  /** Preserve the installed SSB UUID; this is not the website's manifest ID. */
  installId: string;
  name: string;
  startUrl: string;
  /** Preserve the installing tab's context. Zero shares ordinary browser tabs. */
  userContextId: number;
  bundleId: string;
  bundlePath: string;
}

export interface AppShimMigrationPlan {
  transactionId: string;
  profileId: string;
  installId: string;
  bundleId: string;
  bundlePath: string;
  stagedBundlePath: string;
  backupBundlePath: string;
  shimVersion: string;
}

export interface InstalledAppShim {
  transactionId: string;
  shimVersion: string;
  cdHash: string;
  fingerprint: string;
}

export interface AppRegistration extends LegacyAppRegistration {
  integration: "launcher" | "app-shim";
  installedShim?: InstalledAppShim;
  pendingMigration?: AppShimMigrationPlan;
}

export interface AppRegistryState {
  schemaVersion: 1;
  revision: number;
  profile: { id: string; directory: string };
  apps: AppRegistration[];
}

export interface AppRegistryStorage {
  /** Missing file returns null; corrupt or unreadable files must throw. */
  read(): Promise<unknown | null>;
  /** Must durably replace the whole file without exposing a partial JSON file. */
  write(state: AppRegistryState): Promise<void>;
}
