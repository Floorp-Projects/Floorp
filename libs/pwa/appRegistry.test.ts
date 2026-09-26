// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import {
  AppRegistryError,
  AppRegistryStore,
  parseAppRegistry,
  selectMacAppIntegration,
} from "./appRegistry.ts";
import type {
  AppRegistryState,
  AppRegistryStorage,
  LegacyAppRegistration,
  MacAppShimCapabilities,
} from "./appRegistryTypes.ts";

const profileDirectory = "/profiles/通常のプロファイル";
const app: LegacyAppRegistration = {
  installId: "{12345678-1234-5678-1234-567812345678}",
  name: "既存 App",
  startUrl: "https://example.com/app",
  userContextId: 0,
  bundleId: "one.ablaze.floorp.pwa.legacy-token",
  bundlePath: "/Applications/Floorp Apps/既存 App (legacy-token).app",
};
const capabilities: MacAppShimCapabilities = {
  protocolVersion: 1,
  authenticatedTransport: true,
  nativeWindowOwnership: true,
  sharedBrowserProfile: true,
  transactionalInstall: true,
};

class MemoryStorage implements AppRegistryStorage {
  value: unknown | null = null;
  writes = 0;
  reads = 0;
  failWrite = false;

  read(): Promise<unknown | null> {
    this.reads++;
    return Promise.resolve(structuredClone(this.value));
  }

  write(state: AppRegistryState): Promise<void> {
    if (this.failWrite) return Promise.reject(new Error("disk full"));
    this.writes++;
    this.value = structuredClone(state);
    return Promise.resolve();
  }
}

function fixture() {
  const storage = new MemoryStorage();
  const store = new AppRegistryStore(
    storage,
    profileDirectory,
    () => "profile-1",
  );
  return { storage, store };
}

Deno.test("registry preserves the installed identity and default cookie context", async () => {
  const { storage, store } = fixture();
  const first = await store.ensureLegacyApp(app);
  assertEquals(first.profile, { id: "profile-1", directory: profileDirectory });
  assertEquals(first.apps, [{ ...app, integration: "launcher" }]);

  // Reopening a browser must not allocate a new profile or install identity.
  const reopened = new AppRegistryStore(storage, profileDirectory, () => {
    throw new Error("must not regenerate an existing identity");
  });
  assertEquals(await reopened.ensureLegacyApp(app), first);
  assertEquals(storage.writes, 1);
});

Deno.test("registry serializes concurrent apps and preserves container isolation", async () => {
  const { store } = fixture();
  const containerApp = {
    ...app,
    installId: "container-install",
    userContextId: 8,
    bundleId: "one.ablaze.floorp.pwa.container",
    bundlePath: "/Applications/Floorp Apps/Container App.app",
  };
  await Promise.all([
    store.ensureLegacyApp(app),
    store.ensureLegacyApp(containerApp),
    store.ensureLegacyApp(app),
  ]);
  const state = await store.read();
  assertEquals(state?.apps.map((entry) => entry.userContextId), [0, 8]);
  assertEquals(state?.revision, 2);
});

Deno.test("registry refuses implicit profile copy or relocation without overwriting", async () => {
  const { storage, store } = fixture();
  await store.ensureLegacyApp(app);
  const before = structuredClone(storage.value);
  const copied = new AppRegistryStore(
    storage,
    "/profiles/copied",
    () => "new-id",
  );
  const error = await assertRejects(
    () => copied.ensureLegacyApp(app),
    AppRegistryError,
  );
  assertEquals(error.code, "profile-location-mismatch");
  assertEquals(storage.value, before);
});

Deno.test("registry fails closed for corrupt, unsupported, or colliding metadata", async () => {
  const { storage, store } = fixture();
  const valid = await store.ensureLegacyApp(app);
  for (
    const value of [
      null,
      { ...valid, schemaVersion: 2 },
      { ...valid, revision: -1 },
      {
        ...valid,
        apps: [...valid.apps, { ...valid.apps[0], installId: "other" }],
      },
      {
        ...valid,
        apps: [{ ...valid.apps[0], integration: "app-shim" }],
      },
      { ...valid, apps: [{ ...valid.apps[0], userContextId: -1 }] },
      {
        ...valid,
        apps: [{ ...valid.apps[0], bundlePath: "/Apps/../App.app" }],
      },
    ]
  ) {
    assertThrows(() => parseAppRegistry(value), AppRegistryError);
  }
  storage.value = { ...valid, schemaVersion: 7 };
  await assertRejects(() => store.ensureLegacyApp(app), AppRegistryError);
  assertEquals(storage.writes, 1);
});

Deno.test("registry does not silently change an installed app's session identity", async () => {
  const { storage, store } = fixture();
  await store.ensureLegacyApp(app);
  for (
    const changed of [
      { ...app, userContextId: 9 },
      { ...app, startUrl: "https://other.example.com/" },
      { ...app, bundleId: "one.ablaze.floorp.pwa.replaced" },
      { ...app, bundlePath: "/Applications/Renamed.app" },
    ]
  ) {
    const error = await assertRejects(
      () => store.ensureLegacyApp(changed),
      AppRegistryError,
    );
    assertEquals(error.code, "identity-conflict");
  }
  assertEquals(storage.writes, 1);
});

Deno.test("native selection requires all supported Runtime capabilities", () => {
  assertEquals(selectMacAppIntegration(null), "launcher");
  assertEquals(selectMacAppIntegration(undefined), "launcher");
  assertEquals(selectMacAppIntegration(capabilities), "app-shim");
  for (const version of [0, 2]) {
    assertEquals(
      selectMacAppIntegration({ ...capabilities, protocolVersion: version }),
      "launcher",
    );
  }
  for (
    const field of [
      "authenticatedTransport",
      "nativeWindowOwnership",
      "sharedBrowserProfile",
      "transactionalInstall",
    ] as const
  ) {
    assertEquals(
      selectMacAppIntegration({ ...capabilities, [field]: false }),
      "launcher",
    );
  }
});

Deno.test("unsupported Runtime cannot even read or journal a migration", async () => {
  const { storage, store } = fixture();
  const error = await assertRejects(
    () => store.beginMigration(app.installId, "txn-1", "1", null),
    AppRegistryError,
  );
  assertEquals(error.code, "capability-unavailable");
  assertEquals(storage.reads, 0);
  assertEquals(storage.writes, 0);
});

Deno.test("migration journals only intent, retains identity, and survives restart", async () => {
  const { storage, store } = fixture();
  await store.ensureLegacyApp(app);
  const plan = await store.beginMigration(
    app.installId,
    "txn-1",
    "1",
    capabilities,
  );
  assertEquals(plan.bundleId, app.bundleId);
  assertEquals(plan.bundlePath, app.bundlePath);
  assertEquals(plan.profileId, "profile-1");
  assertEquals(
    plan.stagedBundlePath,
    "/Applications/Floorp Apps/.floorp-stage-txn-1.app",
  );
  assertEquals(
    plan.backupBundlePath,
    "/Applications/Floorp Apps/.floorp-backup-txn-1.app",
  );
  const reopened = new AppRegistryStore(storage, profileDirectory);
  const state = await reopened.read();
  assertEquals(state?.apps[0], {
    ...app,
    integration: "launcher",
    pendingMigration: plan,
  });
  assertEquals(
    await reopened.beginMigration(app.installId, "txn-1", "1", capabilities),
    plan,
  );
  assertEquals(storage.writes, 2, "replaying the same intent is idempotent");
  await assertRejects(
    () => reopened.beginMigration(app.installId, "txn-2", "1", capabilities),
    AppRegistryError,
  );
  await assertRejects(
    () => reopened.ensureLegacyApp({ ...app, name: "renamed" }),
    AppRegistryError,
  );
});

Deno.test("migration journal cannot escape siblings or replace another app", async () => {
  const { store } = fixture();
  await store.ensureLegacyApp(app);
  await assertRejects(
    () => store.beginMigration(app.installId, "../escape", "1", capabilities),
    AppRegistryError,
  );
  await store.ensureLegacyApp({
    ...app,
    installId: "second",
    bundleId: "one.ablaze.floorp.pwa.second",
    bundlePath: "/Applications/Floorp Apps/.floorp-stage-txn-1.app",
  });
  await assertRejects(
    () => store.beginMigration(app.installId, "txn-1", "1", capabilities),
    AppRegistryError,
  );
  assertEquals((await store.read())?.apps[0].pendingMigration, undefined);
});

Deno.test("only matching rollback can clear a recovery journal", async () => {
  const { storage, store } = fixture();
  await store.ensureLegacyApp(app);
  await store.beginMigration(app.installId, "txn-1", "1", capabilities);
  const before = structuredClone(storage.value);
  const error = await assertRejects(
    () => store.abortMigration(app.installId, "txn-2"),
    AppRegistryError,
  );
  assertEquals(error.code, "transaction-mismatch");
  assertEquals(storage.value, before);
  await store.abortMigration(app.installId, "txn-1");
  assertEquals((await store.read())?.apps, [{
    ...app,
    integration: "launcher",
  }]);
  await store.abortMigration(app.installId, "txn-1");
  assertEquals(storage.writes, 3);
});

Deno.test("a failed persistent write does not publish identity or poison the queue", async () => {
  const { storage, store } = fixture();
  storage.failWrite = true;
  await assertRejects(() => store.ensureLegacyApp(app), Error, "disk full");
  assertEquals(storage.value, null);
  storage.failWrite = false;
  await store.ensureLegacyApp(app);
  storage.failWrite = true;
  await assertRejects(
    () => store.beginMigration(app.installId, "txn-1", "1", capabilities),
    Error,
    "disk full",
  );
  assertEquals((await store.read())?.apps[0].pendingMigration, undefined);
  storage.failWrite = false;
  await store.beginMigration(app.installId, "txn-1", "1", capabilities);
  assertEquals(storage.writes, 2);
});

Deno.test("native receipt commit preserves session identity and rejects launcher downgrade", async () => {
  const { store } = fixture();
  await store.ensureLegacyApp(app);
  const receipt = {
    transactionId: "native-1",
    shimVersion: "1",
    cdHash: "a".repeat(40),
    fingerprint: "b".repeat(64),
  };
  await assertRejects(
    () => store.completeMigration(app.installId, receipt, app.name),
    AppRegistryError,
  );
  await store.beginMigration(
    app.installId,
    receipt.transactionId,
    "1",
    capabilities,
  );
  await store.completeMigration(app.installId, receipt, app.name);
  const installed = (await store.read())!.apps[0];
  assertEquals(installed, {
    ...app,
    integration: "app-shim",
    installedShim: receipt,
  });
  await assertRejects(() => store.ensureLegacyApp(app), AppRegistryError);
  await assertRejects(
    () => store.restoreInstallation(app.installId, "other", null, app.name),
    AppRegistryError,
  );
  await store.restoreInstallation(
    app.installId,
    receipt.transactionId,
    null,
    app.name,
  );
  assertEquals((await store.read())!.apps[0], {
    ...app,
    integration: "launcher",
  });
});

Deno.test("pending migration journal rejects a late installed receipt", async () => {
  const { store } = fixture();
  await store.ensureLegacyApp(app);
  const installed = {
    transactionId: "native-1",
    shimVersion: "1",
    cdHash: "a".repeat(40),
    fingerprint: "b".repeat(64),
  };
  const pending = {
    ...installed,
    transactionId: "native-2",
    cdHash: "c".repeat(40),
    fingerprint: "d".repeat(64),
  };
  await store.beginMigration(
    app.installId,
    installed.transactionId,
    "1",
    capabilities,
  );
  await store.completeMigration(app.installId, installed, app.name);
  await store.beginMigration(
    app.installId,
    pending.transactionId,
    "1",
    capabilities,
  );
  await assertRejects(
    () => store.completeMigration(app.installId, installed, app.name),
    AppRegistryError,
  );
  const journalled = (await store.read())!.apps[0];
  assertEquals(journalled.pendingMigration?.transactionId, pending.transactionId);
  assertEquals(journalled.installedShim?.transactionId, installed.transactionId);
  await store.completeMigration(app.installId, pending, "New Name");
  const committed = (await store.read())!.apps[0];
  assertEquals(committed.pendingMigration, undefined);
  assertEquals(committed.installedShim, pending);
});

Deno.test("native rename receipts retain bundle and container identities across rollback", async () => {
  const { store } = fixture();
  await store.ensureLegacyApp({ ...app, userContextId: 8 });
  const previous = {
    transactionId: "native-1",
    shimVersion: "1",
    cdHash: "a".repeat(40),
    fingerprint: "b".repeat(64),
  };
  const next = {
    ...previous,
    transactionId: "rename-1",
    cdHash: "c".repeat(40),
    fingerprint: "d".repeat(64),
  };
  await store.beginMigration(
    app.installId,
    previous.transactionId,
    "1",
    capabilities,
  );
  await store.completeMigration(app.installId, previous, app.name);
  await store.beginMigration(
    app.installId,
    next.transactionId,
    "1",
    capabilities,
  );
  await store.completeMigration(app.installId, next, "New Name");
  const renamed = (await store.read())!.apps[0];
  assertEquals(renamed.bundlePath, app.bundlePath);
  assertEquals(renamed.bundleId, app.bundleId);
  assertEquals(renamed.userContextId, 8);
  await store.restoreInstallation(
    app.installId,
    next.transactionId,
    previous,
    app.name,
  );
  assertEquals((await store.read())!.apps[0], {
    ...app,
    userContextId: 8,
    integration: "app-shim",
    installedShim: previous,
  });
});

Deno.test("uninstall forgets only matching bundle ownership and permits explicit reinstall", async () => {
  const { store } = fixture();
  await store.ensureLegacyApp(app);
  await assertRejects(
    () => store.forgetApp(app.installId, "/Applications/Wrong.app"),
    AppRegistryError,
  );
  assertEquals((await store.read())!.apps.length, 1);
  await store.forgetApp(app.installId, app.bundlePath);
  assertEquals((await store.read())!.apps.length, 0);
  await store.ensureLegacyApp({
    ...app,
    name: "Reinstalled",
    bundlePath: "/Applications/Reinstalled.app",
  });
  assertEquals((await store.read())!.apps[0].name, "Reinstalled");
});

Deno.test("explicit legacy container reconciliation preserves identity and refuses pending or native apps", async () => {
  const { store } = fixture();
  await store.ensureLegacyApp(app);
  await store.updateLegacyUserContext(app.installId, 0, 8);
  assertEquals((await store.read())!.apps[0], {
    ...app,
    userContextId: 8,
    integration: "launcher",
  });
  await assertRejects(
    () => store.updateLegacyUserContext(app.installId, 0, 9),
    AppRegistryError,
  );
  await store.beginMigration(app.installId, "native-1", "1", capabilities);
  await assertRejects(
    () => store.updateLegacyUserContext(app.installId, 8, 0),
    AppRegistryError,
  );
  const receipt = {
    transactionId: "native-1",
    shimVersion: "1",
    cdHash: "a".repeat(40),
    fingerprint: "b".repeat(64),
  };
  await store.completeMigration(app.installId, receipt, app.name);
  await assertRejects(
    () => store.updateLegacyUserContext(app.installId, 8, 0),
    AppRegistryError,
  );
  assertEquals((await store.read())!.apps[0].userContextId, 8);
});
