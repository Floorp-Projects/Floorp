// SPDX-License-Identifier: MPL-2.0

import type {
  AppRegistration,
  AppRegistryState,
  AppRegistryStorage,
  AppShimMigrationPlan,
  InstalledAppShim,
  LegacyAppRegistration,
  MacAppShimCapabilities,
} from "./appRegistryTypes.ts";

export type { AppRegistryState, LegacyAppRegistration, MacAppShimCapabilities };

export class AppRegistryError extends Error {
  constructor(
    public readonly code:
      | "invalid-state"
      | "profile-location-mismatch"
      | "identity-conflict"
      | "migration-in-progress"
      | "capability-unavailable"
      | "unknown-app"
      | "transaction-mismatch",
    message: string,
  ) {
    super(`[AppRegistry] ${message}`);
    this.name = "AppRegistryError";
  }
}

export function selectMacAppIntegration(
  capabilities: MacAppShimCapabilities | null | undefined,
): "launcher" | "app-shim" {
  return capabilities?.protocolVersion === 1 &&
      capabilities.authenticatedTransport === true &&
      capabilities.nativeWindowOwnership === true &&
      capabilities.sharedBrowserProfile === true &&
      capabilities.transactionalInstall === true
    ? "app-shim"
    : "launcher";
}

function invalid(message: string): never {
  throw new AppRegistryError("invalid-state", message);
}

function object(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    invalid("Expected an object");
  }
  return value as Record<string, unknown>;
}

function text(value: unknown, field: string): string {
  if (typeof value !== "string" || !value || value.includes("\0")) {
    invalid(`Invalid ${field}`);
  }
  return value;
}

function identifier(value: unknown, field: string): string {
  const result = text(value, field);
  if (result.length > 512 || /[\s/\\]/.test(result)) {
    invalid(`Invalid ${field}`);
  }
  return result;
}

function path(value: unknown, field: string): string {
  const result = text(value, field);
  const components = result.split("/");
  if (
    !result.startsWith("/") || result === "/" ||
    components.slice(1).some((part) => !part || part === "." || part === "..")
  ) {
    invalid(`Expected a normalized absolute macOS path for ${field}`);
  }
  return result;
}

function integer(value: unknown, field: string): number {
  if (typeof value !== "number" || !Number.isSafeInteger(value) || value < 0) {
    invalid(`Invalid ${field}`);
  }
  return value;
}

function registration(value: unknown): LegacyAppRegistration {
  const input = object(value);
  const startUrl = text(input.startUrl, "startUrl");
  let url: URL;
  try {
    url = new URL(startUrl);
  } catch {
    return invalid("Invalid startUrl");
  }
  if (url.protocol !== "https:" && url.protocol !== "http:") {
    invalid("Unsupported startUrl scheme");
  }
  const bundlePath = path(input.bundlePath, "bundlePath");
  const bundleId = text(input.bundleId, "bundleId");
  if (!/^[A-Za-z0-9.-]+$/.test(bundleId) || !bundlePath.endsWith(".app")) {
    invalid("Invalid application bundle identity");
  }
  return {
    installId: identifier(input.installId, "installId"),
    name: text(input.name, "name"),
    startUrl,
    userContextId: integer(input.userContextId, "userContextId"),
    bundleId,
    bundlePath,
  };
}

function migrationPlan(value: unknown): AppShimMigrationPlan {
  const input = object(value);
  const transactionId = identifier(input.transactionId, "transactionId");
  // Used in filenames, unlike arbitrary pre-existing SSB IDs.
  if (!/^[A-Za-z0-9-]{1,80}$/.test(transactionId)) {
    invalid("Invalid migration transaction ID");
  }
  const bundlePath = path(input.bundlePath, "bundlePath");
  const stagedBundlePath = path(input.stagedBundlePath, "stagedBundlePath");
  const backupBundlePath = path(input.backupBundlePath, "backupBundlePath");
  const parent = bundlePath.slice(0, bundlePath.lastIndexOf("/"));
  // Sibling paths let the native installer perform a same-volume exchange.
  if (
    stagedBundlePath !== `${parent}/.floorp-stage-${transactionId}.app` ||
    backupBundlePath !== `${parent}/.floorp-backup-${transactionId}.app`
  ) {
    invalid("Migration paths must be transaction-specific bundle siblings");
  }
  return {
    transactionId,
    profileId: identifier(input.profileId, "profileId"),
    installId: identifier(input.installId, "installId"),
    bundleId: text(input.bundleId, "bundleId"),
    bundlePath,
    stagedBundlePath,
    backupBundlePath,
    shimVersion: identifier(input.shimVersion, "shimVersion"),
  };
}

function installedShim(value: unknown): InstalledAppShim {
  const input = object(value);
  const transactionId = identifier(input.transactionId, "transactionId");
  const cdHash = text(input.cdHash, "cdHash");
  const fingerprint = text(input.fingerprint, "fingerprint");
  if (
    !/^[A-Za-z0-9-]{1,80}$/.test(transactionId) ||
    !/^[a-f0-9]{40,64}$/.test(cdHash) || !/^[a-f0-9]{64}$/.test(fingerprint)
  ) {
    invalid("Invalid native installation receipt");
  }
  return {
    transactionId,
    shimVersion: identifier(input.shimVersion, "shimVersion"),
    cdHash,
    fingerprint,
  };
}

/** Validate persisted metadata before it may participate in a native operation. */
export function parseAppRegistry(value: unknown): AppRegistryState {
  const input = object(value);
  if (input.schemaVersion !== 1 || !Array.isArray(input.apps)) {
    invalid("Unsupported registry schema");
  }
  const profileInput = object(input.profile);
  const profile = {
    id: identifier(profileInput.id, "profileId"),
    directory: path(profileInput.directory, "profileDirectory"),
  };
  const installIds = new Set<string>();
  const bundleIds = new Set<string>();
  const bundlePaths = new Set<string>();
  const transactionIds = new Set<string>();
  const apps = input.apps.map((value): AppRegistration => {
    const source = object(value);
    const app = registration(source);
    if (
      (source.integration !== "launcher" &&
        source.integration !== "app-shim") ||
      installIds.has(app.installId) ||
      bundleIds.has(app.bundleId) || bundlePaths.has(app.bundlePath)
    ) {
      invalid("Duplicate or unsupported application identity");
    }
    installIds.add(app.installId);
    bundleIds.add(app.bundleId);
    bundlePaths.add(app.bundlePath);
    const result: AppRegistration = { ...app, integration: source.integration };
    if (source.integration === "app-shim") {
      result.installedShim = installedShim(source.installedShim);
    } else if (source.installedShim !== undefined) {
      invalid("Launcher cannot carry a native installation receipt");
    }
    if (source.pendingMigration !== undefined) {
      const migration = migrationPlan(source.pendingMigration);
      if (
        migration.profileId !== profile.id ||
        migration.installId !== app.installId ||
        migration.bundleId !== app.bundleId ||
        migration.bundlePath !== app.bundlePath ||
        transactionIds.has(migration.transactionId)
      ) {
        invalid("Migration does not match its registered application");
      }
      transactionIds.add(migration.transactionId);
      result.pendingMigration = migration;
    }
    return result;
  });
  for (const app of apps) {
    const migration = app.pendingMigration;
    if (
      migration && (bundlePaths.has(migration.stagedBundlePath) ||
        bundlePaths.has(migration.backupBundlePath))
    ) {
      invalid("Migration staging path conflicts with an installed application");
    }
  }
  return {
    schemaVersion: 1,
    revision: integer(input.revision, "revision"),
    profile,
    apps,
  };
}

/**
 * One instance per profile in the browser parent process. The adapter must not
 * silently replace an unreadable registry or permit a Shim to open profile data.
 */
export class AppRegistryStore {
  private pending: Promise<void> = Promise.resolve();

  constructor(
    private readonly storage: AppRegistryStorage,
    private readonly profileDirectory: string,
    private readonly newProfileId: () => string = () => crypto.randomUUID(),
  ) {
    path(profileDirectory, "profileDirectory");
  }

  private enqueue<T>(operation: () => Promise<T>): Promise<T> {
    const result = this.pending.then(operation);
    this.pending = result.then(() => {}, () => {});
    return result;
  }

  private async load(): Promise<AppRegistryState | null> {
    const raw = await this.storage.read();
    if (raw === null) return null;
    const state = parseAppRegistry(raw);
    if (state.profile.directory !== this.profileDirectory) {
      throw new AppRegistryError(
        "profile-location-mismatch",
        "Profile moved or was copied; explicit identity reconciliation is required",
      );
    }
    return state;
  }

  private async write(state: AppRegistryState): Promise<void> {
    state.revision++;
    await this.storage.write(parseAppRegistry(state));
  }

  read(): Promise<AppRegistryState | null> {
    return this.enqueue(() => this.load());
  }

  ensureLegacyApp(input: LegacyAppRegistration): Promise<AppRegistryState> {
    return this.enqueue(async () => {
      const app = registration(input);
      const state = await this.load() ?? {
        schemaVersion: 1,
        revision: 0,
        profile: {
          id: identifier(this.newProfileId(), "profileId"),
          directory: this.profileDirectory,
        },
        apps: [],
      } satisfies AppRegistryState;
      const existing = state.apps.find((entry) =>
        entry.installId === app.installId
      );
      if (existing) {
        // Preserve all launch/session identity. Explicit container changes and
        // renames need reconciliation against the live store, not guessed here.
        if (
          existing.bundleId !== app.bundleId ||
          existing.bundlePath !== app.bundlePath ||
          existing.userContextId !== app.userContextId ||
          existing.startUrl !== app.startUrl
        ) {
          throw new AppRegistryError(
            "identity-conflict",
            "Registered application identity changed; refusing implicit migration",
          );
        }
        if (existing.integration !== "launcher") {
          throw new AppRegistryError(
            "identity-conflict",
            "A signed native installation cannot be enrolled as a launcher",
          );
        }
        if (existing.name === app.name) return state;
        if (existing.pendingMigration) {
          throw new AppRegistryError(
            "migration-in-progress",
            "Cannot change metadata during a pending migration",
          );
        }
        existing.name = app.name;
      } else {
        state.apps.push({ ...app, integration: "launcher" });
      }
      await this.write(state);
      return state;
    });
  }

  /**
   * Journal intent only. This never creates, replaces, or removes an app bundle
   * and never marks a Shim as active. The native installer owns those actions.
   */
  beginMigration(
    installId: string,
    transactionId: string,
    shimVersion: string,
    capabilities: MacAppShimCapabilities | null,
  ): Promise<AppShimMigrationPlan> {
    return this.enqueue(async () => {
      if (selectMacAppIntegration(capabilities) !== "app-shim") {
        throw new AppRegistryError(
          "capability-unavailable",
          "Runtime cannot install independent shared-profile Web Apps",
        );
      }
      const state = await this.load();
      const app = state?.apps.find((entry) => entry.installId === installId);
      if (!state || !app) {
        throw new AppRegistryError(
          "unknown-app",
          "Application is not registered",
        );
      }
      const parent = app.bundlePath.slice(0, app.bundlePath.lastIndexOf("/"));
      const plan = migrationPlan({
        transactionId,
        shimVersion,
        profileId: state.profile.id,
        installId: app.installId,
        bundleId: app.bundleId,
        bundlePath: app.bundlePath,
        stagedBundlePath: `${parent}/.floorp-stage-${transactionId}.app`,
        backupBundlePath: `${parent}/.floorp-backup-${transactionId}.app`,
      });
      if (app.pendingMigration) {
        if (JSON.stringify(app.pendingMigration) === JSON.stringify(plan)) {
          return app.pendingMigration;
        }
        throw new AppRegistryError(
          "migration-in-progress",
          "A previous migration must be recovered before preparing another",
        );
      }
      app.pendingMigration = plan;
      await this.write(state);
      return plan;
    });
  }

  /** Call only after the native installer confirms the old bundle is restored. */
  abortMigration(installId: string, transactionId: string): Promise<void> {
    return this.enqueue(async () => {
      const state = await this.load();
      const app = state?.apps.find((entry) => entry.installId === installId);
      if (!state || !app) {
        throw new AppRegistryError(
          "unknown-app",
          "Application is not registered",
        );
      }
      if (!app.pendingMigration) return;
      if (app.pendingMigration.transactionId !== transactionId) {
        throw new AppRegistryError(
          "transaction-mismatch",
          "Cannot clear another migration's recovery journal",
        );
      }
      delete app.pendingMigration;
      await this.write(state);
    });
  }

  /** Only the native installer may call this after verifying the exchanged app. */
  completeMigration(
    installId: string,
    receipt: InstalledAppShim,
    name: string,
  ): Promise<void> {
    return this.enqueue(async () => {
      const verified = installedShim(receipt);
      const state = await this.load();
      const app = state?.apps.find((entry) => entry.installId === installId);
      if (!state || !app) {
        throw new AppRegistryError(
          "unknown-app",
          "Application is not registered",
        );
      }
      // A pending migration owns the journal: only its own transaction may
      // commit it. Without one, only an idempotent replay of the installed
      // receipt is accepted, so a late receipt cannot delete a live journal.
      const expected = app.pendingMigration?.transactionId ??
        app.installedShim?.transactionId;
      if (expected !== verified.transactionId) {
        throw new AppRegistryError(
          "transaction-mismatch",
          "Native receipt does not match the prepared transaction",
        );
      }
      app.integration = "app-shim";
      app.installedShim = verified;
      app.name = text(name, "name");
      delete app.pendingMigration;
      await this.write(state);
    });
  }

  /** Restore a previous receipt only after its bundle has been restored. */
  restoreInstallation(
    installId: string,
    transactionId: string,
    previous: InstalledAppShim | null,
    name: string,
  ): Promise<void> {
    return this.enqueue(async () => {
      const state = await this.load();
      const app = state?.apps.find((entry) => entry.installId === installId);
      if (!state || !app) {
        throw new AppRegistryError(
          "unknown-app",
          "Application is not registered",
        );
      }
      const current = app.pendingMigration?.transactionId ??
        app.installedShim?.transactionId;
      if (
        current !== undefined && current !== transactionId &&
        current !== previous?.transactionId
      ) {
        throw new AppRegistryError(
          "transaction-mismatch",
          "Cannot restore a different native transaction",
        );
      }
      app.integration = previous ? "app-shim" : "launcher";
      if (previous) app.installedShim = installedShim(previous);
      else delete app.installedShim;
      app.name = text(name, "name");
      delete app.pendingMigration;
      await this.write(state);
    });
  }

  /** Remove metadata only after the corresponding owned bundle was removed. */
  forgetApp(installId: string, bundlePath: string): Promise<void> {
    return this.enqueue(async () => {
      const state = await this.load();
      const app = state?.apps.find((entry) => entry.installId === installId);
      if (!state || !app) return;
      if (app.bundlePath !== bundlePath) {
        throw new AppRegistryError(
          "identity-conflict",
          "Uninstall path does not match the registered application",
        );
      }
      state.apps = state.apps.filter((entry) => entry !== app);
      await this.write(state);
    });
  }

  /** Reconcile an explicit canonical-store context change, never a native app. */
  updateLegacyUserContext(
    installId: string,
    previousContext: number,
    nextContext: number,
  ): Promise<void> {
    return this.enqueue(async () => {
      integer(previousContext, "previousContext");
      integer(nextContext, "nextContext");
      const state = await this.load();
      const app = state?.apps.find((entry) => entry.installId === installId);
      if (!state || !app) return;
      if (app.integration !== "launcher" || app.pendingMigration) {
        throw new AppRegistryError(
          "migration-in-progress",
          "A native app's session context cannot be changed",
        );
      }
      if (app.userContextId === nextContext) return;
      if (app.userContextId !== previousContext) {
        throw new AppRegistryError(
          "identity-conflict",
          "Previous container does not match the registered app",
        );
      }
      app.userContextId = nextContext;
      await this.write(state);
    });
  }
}
