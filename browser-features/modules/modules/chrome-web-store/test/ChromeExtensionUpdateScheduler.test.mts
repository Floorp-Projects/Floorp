// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { unzipSync, zipSync } from "fflate/browser";
import {
  ChromeExtensionUpdateRegistryClass,
  createVerifiedUpdateProof,
} from "../ChromeExtensionUpdateRegistry.sys.mts";
import { ChromeExtensionUpdaterClass } from "../ChromeExtensionUpdater.sys.mts";
import { ChromeExtensionUpdateSchedulerClass } from "../ChromeExtensionUpdateScheduler.sys.mts";
import {
  assertWorkerReplaced,
  browser,
  command,
  fixture,
  ID,
  manager,
  observePrompts,
  packageVersion,
  rejected,
  waitFor,
  workerSnapshot,
} from "./ChromeExtensionUpdater.test.mts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function registryFixture(
  fn: (
    registry: ChromeExtensionUpdateRegistryClass,
    path: string,
  ) => Promise<void>,
): Promise<void> {
  const directory = PathUtils.join(
    PathUtils.tempDir,
    `floorp-update-registry-${Services.uuid.generateUUID()}`,
  );
  const path = PathUtils.join(directory, "registrations.json");
  const updatePref = "extensions.update.enabled";
  const hadUpdatePref = Services.prefs.prefHasUserValue(updatePref);
  const updatesEnabled = Services.prefs.getBoolPref(updatePref, true);
  Services.prefs.setBoolPref(updatePref, true);
  try {
    await fn(new ChromeExtensionUpdateRegistryClass(path), path);
  } finally {
    if (hadUpdatePref) Services.prefs.setBoolPref(updatePref, updatesEnabled);
    else Services.prefs.clearUserPref(updatePref);
    await IOUtils.remove(directory, { recursive: true, ignoreAbsent: true });
  }
}

async function waitAsync(
  read: () => Promise<boolean>,
  name: string,
): Promise<void> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    if (await read()) return;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${name}`);
}

const tests: TestCase[] = [
  {
    name:
      "marker and report copies cannot enroll automatic updates; completed verified conversion can",
    async fn() {
      await fixture((converted) =>
        registryFixture(async (registry, path) => {
          let downloads = 0;
          const v2 = await packageVersion("2.0");
          const updater = new ChromeExtensionUpdaterClass(() => {
            downloads++;
            return Promise.resolve(v2);
          }, registry);
          const scheduler = new ChromeExtensionUpdateSchedulerClass({
            registry,
            updater,
          });
          assertEquals(
            (await scheduler.getStatus(ID)).eligibility,
            "migration-required",
            "packaged marker and signature report are insufficient",
          );
          await rejected(
            scheduler.setAutomaticUpdates(ID, true),
            "Explicit verified migration",
          );
          await scheduler.runDueUpdates({ force: true });
          assertEquals(
            downloads,
            0,
            "unregistered installed XPI never auto-checks",
          );
          const proof = await createVerifiedUpdateProof(
            ID,
            "1.0",
            converted.xpiData,
            converted.report.signature!.publicKeySha256,
          );
          await registry.recordInstalled(proof);
          assertEquals(
            (await registry.getEligibility(ID)).eligibility,
            "verified",
            "completed authenticated installer registration binds actual XPI bytes",
          );
          const reloaded = new ChromeExtensionUpdateRegistryClass(path);
          assertEquals(
            (await reloaded.getEligibility(ID)).eligibility,
            "verified",
            "authorization persists in profile registry",
          );
          await command("set", "automatic-update-storage");
          const beforeWorker = await workerSnapshot();
          const before = (await manager().getAddonByID(ID))!.userPermissions;
          await scheduler.runDueUpdates({ force: true });
          assertEquals(
            (await manager().getAddonByID(ID))?.version,
            "2.0",
            "automatic native update actually installed",
          );
          await assertWorkerReplaced(beforeWorker, "2.0");
          assertEquals(
            (await command("get")).value,
            "automatic-update-storage",
            "scheduled update retains real storage",
          );
          assertEquals(
            JSON.stringify((await manager().getAddonByID(ID))!.userPermissions),
            JSON.stringify(before),
            "native grants retained",
          );
          const status = await scheduler.getStatus(ID);
          assertEquals(
            status.state,
            "installed",
            "management status records native completion",
          );
          assertEquals(
            status.eligibility,
            "verified",
            "new installed archive hash replaces old registration",
          );
          assert(
            status.nextCheck! > status.lastCheck!,
            "successful update schedules next periodic check",
          );
        })
      );
    },
  },
  {
    name:
      "modifying an installed XPI invalidates automatic eligibility even when its marker remains",
    async fn() {
      await fixture((converted) =>
        registryFixture(async (registry) => {
          await registry.recordInstalled(
            await createVerifiedUpdateProof(
              ID,
              "1.0",
              converted.xpiData,
              converted.report.signature!.publicKeySha256,
            ),
          );
          const addon = await manager().getAddonByID(ID);
          assert(addon, "native fixture installed");
          const file = Services.io.newURI(addon.getResourceURI("").spec)
            .QueryInterface!(Ci.nsIJARURI).JARFile.QueryInterface!(
              Ci.nsIFileURL,
            ).file;
          const original = await IOUtils.read(file.path);
          const entries = unzipSync(original);
          const tampered = zipSync({
            ...entries,
            "floorp-owned-tamper.txt": new TextEncoder().encode(
              "Owned archive-integrity regression entry",
            ),
          });
          const modifiedEntries = unzipSync(tampered);
          for (const [name, bytes] of Object.entries(entries)) {
            assertEquals(
              JSON.stringify(Array.from(modifiedEntries[name])),
              JSON.stringify(Array.from(bytes)),
              `tampering preserves original archive entry ${name}`,
            );
          }
          assert(
            modifiedEntries["floorp-owned-tamper.txt"],
            "modified archive contains the additional owned entry",
          );
          await command("set", "tamper-eligibility-state");
          const replaceArchive = async (bytes: Uint8Array): Promise<void> => {
            const before = await workerSnapshot();
            await addon.disable();
            await waitFor(
              () => before.context.unloaded,
              "disabled extension's actual old context shutdown",
            );
            assert(
              !addon.isActive,
              "native addon is disabled during archive mutation",
            );
            // Gecko's own generated-XPI cleanup awaits this cross-process flush
            // after shutdown before replacing a mapped archive on Windows.
            await before.extension.broadcast("Extension:FlushJarCache", {
              path: file.path,
            });
            Services.obs.notifyObservers(file, "flush-cache-entry");
            try {
              await IOUtils.write(file.path, bytes);
            } finally {
              await addon.enable();
            }
            await assertWorkerReplaced(before, "1.0");
            assert(
              addon.isActive,
              "native addon is active again before eligibility checks",
            );
            assertEquals(
              (await command("get")).value,
              "tamper-eligibility-state",
              "native disable/enable preserves real extension storage",
            );
          };
          let downloads = 0;
          const scheduler = new ChromeExtensionUpdateSchedulerClass({
            registry,
            updater: new ChromeExtensionUpdaterClass(() => {
              downloads++;
              return packageVersion("2.0");
            }, registry),
          });
          assertEquals(
            (await registry.getEligibility(ID)).eligibility,
            "verified",
            "original installed archive starts eligible",
          );
          try {
            await replaceArchive(tampered);
            assertEquals(
              (await scheduler.getStatus(ID)).eligibility,
              "migration-required",
              "hash mismatch fails closed",
            );
            await scheduler.runDueUpdates({ force: true });
            assertEquals(
              downloads,
              0,
              "changed installed package cannot use protected authorization",
            );
          } finally {
            await replaceArchive(original);
          }
          assertEquals(
            (await registry.getEligibility(ID)).eligibility,
            "verified",
            "restoring the exact original archive restores its protected hash binding",
          );
        })
      );
    },
  },
  {
    name:
      "automatic opt-out and no-browser deferral preserve the installed extension",
    async fn() {
      await fixture((converted) =>
        registryFixture(async (registry) => {
          await registry.recordInstalled(
            await createVerifiedUpdateProof(
              ID,
              "1.0",
              converted.xpiData,
              converted.report.signature!.publicKeySha256,
            ),
          );
          let downloads = 0;
          let target: Element | null = null;
          const v2 = await packageVersion("2.0");
          const scheduler = new ChromeExtensionUpdateSchedulerClass({
            registry,
            browser: () => target,
            updater: new ChromeExtensionUpdaterClass(() => {
              downloads++;
              return Promise.resolve(v2);
            }, registry),
          });
          Services.prefs.setBoolPref("extensions.update.enabled", false);
          await scheduler.runDueUpdates({ force: true });
          assertEquals(
            downloads,
            0,
            "native global update preference prevents scheduled checks",
          );
          assertEquals(
            (await scheduler.getStatus(ID)).policyAllowed,
            false,
            "management status explains native update policy",
          );
          Services.prefs.setBoolPref("extensions.update.enabled", true);
          await scheduler.setAutomaticUpdates(ID, false);
          await scheduler.runDueUpdates({ force: true });
          assertEquals(downloads, 0, "per-extension opt-out prevents checks");
          await scheduler.setAutomaticUpdates(ID, true);
          await scheduler.runDueUpdates({ force: true });
          assertEquals(
            (await scheduler.getStatus(ID)).state,
            "waiting-for-window",
            "no hidden installation without browser confirmation context",
          );
          assertEquals(
            (await manager().getAddonByID(ID))?.version,
            "1.0",
            "no-window deferral retains old version",
          );
          const beforeWorker = await workerSnapshot();
          target = browser();
          await scheduler.runDueUpdates();
          assertEquals(
            (await manager().getAddonByID(ID))?.version,
            "2.0",
            "waiting update resumes through normal browser flow",
          );
          await assertWorkerReplaced(beforeWorker, "2.0");
        })
      );
    },
  },
  {
    name:
      "scheduled permission increases use the native prompt and rejected updates back off",
    async fn() {
      await fixture((converted) =>
        registryFixture(async (registry) => {
          await registry.recordInstalled(
            await createVerifiedUpdateProof(
              ID,
              "1.0",
              converted.xpiData,
              converted.report.signature!.publicKeySha256,
            ),
          );
          const v2 = await packageVersion("2.0", {
            permissions: ["storage", "bookmarks"],
          });
          let downloads = 0;
          let prompted = false;
          const scheduler = new ChromeExtensionUpdateSchedulerClass({
            registry,
            updater: new ChromeExtensionUpdaterClass(() => {
              downloads++;
              return Promise.resolve(v2);
            }, registry),
          });
          const stop = observePrompts(
            "webextension-update-permission-prompt",
            (prompt) => {
              prompted = true;
              prompt.reject();
            },
          );
          try {
            await scheduler.runDueUpdates({ force: true });
          } finally {
            stop();
          }
          assert(
            prompted,
            "native permission-difference prompt actually emitted",
          );
          assertEquals(
            (await manager().getAddonByID(ID))?.version,
            "1.0",
            "permission rejection prevents replacement",
          );
          const status = await scheduler.getStatus(ID);
          assertEquals(
            status.state,
            "error",
            "rejection visible in management status",
          );
          assert(
            status.error?.includes("cancelled"),
            "actionable native rejection reason",
          );
          assert(
            status.nextCheck! > status.lastCheck!,
            "failure has a retry delay",
          );
          await scheduler.runDueUpdates();
          assertEquals(
            downloads,
            1,
            "rejected update is not immediately retried",
          );
        })
      );
    },
  },
  {
    name:
      "explicit verified reconversion migrates a legacy installation without changing identity or storage",
    async fn() {
      await fixture(() =>
        registryFixture(async (registry) => {
          await command("set", "legacy-migration-data");
          const updater = new ChromeExtensionUpdaterClass(
            async () => await packageVersion("1.0"),
            registry,
          );
          const scheduler = new ChromeExtensionUpdateSchedulerClass({
            registry,
            updater,
          });
          assertEquals(
            (await scheduler.getStatus(ID)).eligibility,
            "migration-required",
            "legacy conversion starts unregistered",
          );
          const check = await updater.checkForUpdate(ID, { reconvert: true });
          assertEquals(
            check.kind,
            "reconvert",
            "migration explicitly prepares same version",
          );
          assertEquals(
            (await scheduler.getStatus(ID)).eligibility,
            "migration-required",
            "preparing a candidate does not authorize old installed XPI",
          );
          const beforeWorker = await workerSnapshot();
          await updater.installUpdate(ID, browser());
          await assertWorkerReplaced(beforeWorker, "1.0");
          assertEquals(
            (await scheduler.getStatus(ID)).eligibility,
            "verified",
            "only completed native migration authorizes scheduling",
          );
          assertEquals(
            (await command("get")).value,
            "legacy-migration-data",
            "same-version migration preserves stored data",
          );
          assertEquals(
            (await manager().getAddonByID(ID))?.id,
            ID,
            "native addon identity retained",
          );
        })
      );
    },
  },
  {
    name:
      "approved postponed proof survives registry reload and authorizes only the exact resumed package",
    async fn() {
      await fixture(
        (converted) =>
          registryFixture(async (registry, path) => {
            await registry.recordInstalled(
              await createVerifiedUpdateProof(
                ID,
                "1.0",
                converted.xpiData,
                converted.report.signature!.publicKeySha256,
              ),
            );
            const v2 = await packageVersion("2.0");
            const updater = new ChromeExtensionUpdaterClass(
              () => Promise.resolve(v2),
              registry,
            );
            const scheduler = new ChromeExtensionUpdateSchedulerClass({
              registry,
              updater,
            });
            const beforeWorker = await workerSnapshot();
            await scheduler.runDueUpdates({ force: true });
            assertEquals(
              (await scheduler.getStatus(ID)).state,
              "postponed",
              "accepted native deferred update remains pending",
            );
            const recovered = new ChromeExtensionUpdateRegistryClass(path);
            const before = await recovered.getEligibility(ID);
            assertEquals(
              before.pendingVersion,
              "2.0",
              "accepted candidate proof persisted before returning",
            );
            assertEquals(
              before.registration?.version,
              "1.0",
              "old running package remains independently bound",
            );
            const postponedReply = await command("get");
            assertEquals(
              postponedReply.contextId,
              beforeWorker.contextId,
              "scheduled postponement retains the running Worker",
            );
            assertEquals(
              postponedReply.pending,
              "2.0",
              "the actual Worker received the scheduled update event",
            );
            assert(
              !beforeWorker.context.unloaded,
              "scheduled postponement leaves the old Worker running",
            );
            const pending = (await manager().getAllInstalls()).find(
              (install) =>
                install.addon?.id === ID &&
                install.state === manager().STATE_POSTPONED,
            );
            assert(pending, "native deferred installation exists");
            await new Promise<void>((resolve, reject) => {
              const finish = (error?: Error) => {
                clearTimeout(timer);
                pending.removeListener(listener);
                if (error) reject(error);
                else resolve();
              };
              const listener = {
                onInstallEnded() {
                  finish();
                },
                onInstallFailed() {
                  finish(new Error("Native postponed resume failed"));
                },
                onInstallCancelled() {
                  finish(new Error("Native postponed resume cancelled"));
                },
              };
              const timer = setTimeout(
                () => finish(new Error("Native postponed resume timed out")),
                15000,
              );
              pending.addListener(listener);
              pending.continuePostponedInstall();
            });
            await assertWorkerReplaced(beforeWorker, "2.0");
            await waitAsync(async () => {
              const saved = (await IOUtils.readJSON(path)) as {
                registrations: Array<{ version: string }>;
                pending: unknown[];
              };
              return (
                saved.registrations[0]?.version === "2.0" &&
                saved.pending.length === 0
              );
            }, "terminal authorization persistence");
            assertEquals(
              (await recovered.getEligibility(ID)).registration?.version,
              "2.0",
              "reloaded accepted proof promotes only matching actual archive",
            );
            assertEquals(
              (await scheduler.getStatus(ID)).state,
              "installed",
              "management status follows deferred completion",
            );
          }),
        { postpone: true },
      );
    },
  },
  {
    name:
      "startup timer runs registered updates and stop removes the scheduler timer",
    async fn() {
      await fixture((converted) =>
        registryFixture(async (registry) => {
          await registry.recordInstalled(
            await createVerifiedUpdateProof(
              ID,
              "1.0",
              converted.xpiData,
              converted.report.signature!.publicKeySha256,
            ),
          );
          const v2 = await packageVersion("2.0");
          let downloads = 0;
          const scheduler = new ChromeExtensionUpdateSchedulerClass({
            registry,
            startupDelayMs: 10,
            timerIntervalMs: 100000,
            updater: new ChromeExtensionUpdaterClass(() => {
              downloads++;
              return Promise.resolve(v2);
            }, registry),
          });
          const beforeWorker = await workerSnapshot();
          scheduler.start();
          try {
            await waitAsync(
              async () => (await scheduler.getStatus(ID)).state === "installed",
              "automatic startup update",
            );
          } finally {
            await scheduler.stop();
          }
          assertEquals(downloads, 1, "timer-triggered update runs once");
          await assertWorkerReplaced(beforeWorker, "2.0");
        })
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("ChromeExtensionUpdateScheduler.test.mts", tests);
}
