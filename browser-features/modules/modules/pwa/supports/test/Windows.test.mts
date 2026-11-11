// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import type { WindowsSupport } from "../Windows.sys.mts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import {
  deferred,
  recordWindowsRaceEvidence,
  until,
  within,
  withWindowsRaceFixture,
} from "./windowsRaceTestUtils.ts";
import type {
  MockShell,
  StoreAwareSupport,
  StoreAwareWindowsSupport,
  WindowsCoordinator,
} from "./types.ts";

function coordinator(Support: typeof WindowsSupport): WindowsCoordinator {
  return Support as unknown as WindowsCoordinator;
}

export async function runAllTests(): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  if (AppConstants.platform !== "win") {
    console.info("[Windows.test] Native Windows is required; skipped");
    return;
  }
  await runTests("Windows.test.mts", [
    {
      name:
        "uninstall cancels a pending delayed pin prompt from another instance",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store, key } = fixture;
          const integration = fixture.track(
            new Support().applyOSIntegration(
              { ...app },
              globalThis as unknown as Window,
            ),
          );
          await until(
            async () =>
              (await store.getCurrentSsbData())[key]?.pinPromptShown === true,
            "launch did not reach the persisted delayed pin prompt",
          );
          const removal = fixture.removeLikeManager(new Support());
          await within(
            Promise.all([integration, removal]),
            "delayed prompt teardown stalled",
          );
          await fixture.assertRemoved();
        }),
    },
    {
      name:
        "uninstall during icon loading prevents stale manifest saves and repair",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app } = fixture;
          const icons = fixture.pauseIconLoads();
          const integration = fixture.track(
            new Support().applyOSIntegration(
              { ...app },
              globalThis as unknown as Window,
            ),
          );
          await within(icons.entered, "launch did not reach both icon reads");
          const removal = fixture.removeLikeManager(new Support());
          // Uncoordinated removal starts immediately. Let it finish before the
          // held icon load; coordinated removal instead waits behind integration.
          if (fixture.unpinCalls.length > 0) {
            await within(
              removal,
              "started removal stalled while icons were held",
            );
          }
          icons.release();
          await within(
            Promise.all([integration, removal]),
            "icon-load teardown stalled",
          );
          await fixture.assertRemoved();
        }),
    },

    {
      name:
        "disabled native integration still installs and removes registration",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store, key } = fixture;
          await fixture.removeLikeManager(new Support());
          const support = new Support() as unknown as StoreAwareWindowsSupport;
          await fixture.track(support.install({ ...app }, store, false));
          assertEquals(
            (await store.getCurrentSsbData())[key]?.id,
            app.id,
            "integration-disabled install commits the registration",
          );
          assert(
            !await IOUtils.exists(fixture.shortcut),
            "integration-disabled install creates no shortcut",
          );
          assert(
            !await IOUtils.exists(fixture.directory),
            "integration-disabled install creates no icon directory",
          );
          assertEquals(
            fixture.pinCalls.length,
            0,
            "integration-disabled install requests no pin UI",
          );
          await fixture.track(support.uninstall({ ...app }, store, false));
          await fixture.assertRemoved();
        }),
    },
    {
      name: "a held launch does not block another app's native installation",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store } = fixture;
          const other = await fixture.ownApp("independent");
          const icons = fixture.pauseIconLoads();
          const integration = fixture.track(
            new Support().applyOSIntegration(
              { ...app },
              globalThis as unknown as Window,
            ),
          );
          try {
            await within(
              icons.entered,
              "first app did not reach icon preparation",
            );
            const installer =
              new Support() as unknown as StoreAwareWindowsSupport;
            await within(
              fixture.track(installer.install(other.app, store)),
              "a held app blocked another app's native installation",
            );
            assert(
              await IOUtils.exists(other.shortcut),
              "independent native shortcut is created while the first app is held",
            );
            assert(
              await IOUtils.exists(other.directory),
              "independent icon directory is created",
            );
            assertEquals(
              (await store.getCurrentSsbData())[other.key]?.id,
              other.app.id,
              "independent app is committed in the shared store",
            );
            icons.release();
            await within(integration, "first app did not resume");
          } finally {
            icons.release();
          }
        }),
    },
    {
      name: "an app mutation blocks its own successor but not another app",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const queue = coordinator(fixture.Support);
          const entered = deferred();
          const release = deferred();
          const first = fixture.track(
            queue.withMutation(fixture.app.id, async () => {
              entered.resolve();
              await release.promise;
            }),
          );
          let sameAppRan = false;
          let otherAppRan = false;
          try {
            await within(entered.promise, "first app mutation did not start");
            const successor = fixture.track(
              queue.withMutation(fixture.app.id, () => {
                sameAppRan = true;
                return Promise.resolve();
              }),
            );
            const independent = fixture.track(
              queue.withMutation(`${fixture.app.id}-other`, () => {
                otherAppRan = true;
                return Promise.resolve();
              }),
            );
            await within(
              independent,
              "one app blocked an unrelated app mutation",
            );
            assert(
              otherAppRan,
              "unrelated app completed while first app was held",
            );
            assert(!sameAppRan, "same-app successor waits for its predecessor");
            release.resolve();
            await within(
              Promise.all([first, successor]),
              "same-app queue did not drain",
            );
            assert(sameAppRan, "same-app successor runs after release");
          } finally {
            release.resolve();
          }
        }),
    },
    {
      name:
        "repeated uninstall is idempotent and explicit same-ID reinstall works",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store, key } = fixture;
          await fixture.removeLikeManager(new Support());
          await fixture.removeLikeManager(new Support());
          await fixture.assertRemoved();

          const reinstall =
            new Support() as unknown as StoreAwareWindowsSupport;
          await fixture.track(reinstall.install({ ...app }, store));
          assert(
            await IOUtils.exists(fixture.shortcut),
            "reinstall creates the native shortcut",
          );
          assert(
            await IOUtils.exists(fixture.directory),
            "reinstall creates its icon directory",
          );
          assertEquals(
            (await store.getCurrentSsbData())[key]?.id,
            app.id,
            "reinstall restores the same app ID in the store",
          );

          fixture.pinCalls.length = 0;
          await fixture.removeLikeManager(new Support());
          await fixture.assertRemoved();
        }),
    },
    {
      name:
        "explicit reinstall follows a cancelled launch and uninstall in order",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store, key } = fixture;
          const icons = fixture.pauseIconLoads();
          const integration = fixture.track(
            new Support().applyOSIntegration(
              { ...app },
              globalThis as unknown as Window,
            ),
          );
          try {
            await within(
              icons.entered,
              "old launch did not enter icon preparation",
            );
            const remover =
              new Support() as unknown as StoreAwareWindowsSupport;
            const installer =
              new Support() as unknown as StoreAwareWindowsSupport;
            const removal = fixture.track(remover.uninstall({ ...app }, store));
            const installation = fixture.track(
              installer.install({ ...app }, store),
            );
            icons.release();
            await within(
              Promise.all([integration, removal, installation]),
              "uninstall followed by explicit reinstall stalled",
            );
            assert(
              await IOUtils.exists(fixture.shortcut),
              "explicit reinstall owns the final native shortcut",
            );
            assert(
              await IOUtils.exists(fixture.directory),
              "explicit reinstall owns the final icon directory",
            );
            assertEquals(
              (await store.getCurrentSsbData())[key]?.id,
              app.id,
              "explicit reinstall owns the final store entry",
            );
            assertEquals(
              fixture.pinCalls.length,
              1,
              "only the explicit reinstall requests pinning",
            );
          } finally {
            icons.release();
          }
        }),
    },
    {
      name:
        "uninstall queued after rename removes the renamed shortcut and store",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store, key } = fixture;
          const newName = `${app.name}-renamed`;
          const renamedShortcut = await fixture.ownName(newName);
          const renamer = new Support() as unknown as StoreAwareWindowsSupport;
          const remover = new Support() as unknown as StoreAwareWindowsSupport;
          const rename = fixture.track(
            renamer.rename({ ...app }, newName, store),
          );
          const removal = fixture.track(remover.uninstall({ ...app }, store));
          await within(
            Promise.all([rename, removal]),
            "rename followed by uninstall stalled",
          );
          assert(
            !await IOUtils.exists(fixture.shortcut),
            "the original shortcut is absent",
          );
          assert(
            !await IOUtils.exists(renamedShortcut),
            "the renamed shortcut is absent",
          );
          assert(
            !await IOUtils.exists(fixture.directory),
            "the app icon directory is absent",
          );
          assert(
            !(key in await store.getCurrentSsbData()),
            "rename cannot resurrect the removed store entry",
          );
        }),
    },
    {
      name:
        "a late captured launch manifest cannot reinstall an uninstalled app",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app } = fixture;
          await fixture.removeLikeManager(new Support());
          await fixture.track(
            new Support().applyOSIntegration(
              { ...app },
              globalThis as unknown as Window,
            ),
          );
          await fixture.assertRemoved();
        }),
    },
    {
      name:
        "pin-prompt persistence preserves metadata edited during icon loading",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store, key } = fixture;
          const icons = fixture.pauseIconLoads();
          const integration = fixture.track(
            new Support().applyOSIntegration(
              { ...app },
              globalThis as unknown as Window,
            ),
          );
          try {
            await within(
              icons.entered,
              "launch did not enter icon preparation",
            );
            await within(
              coordinator(Support).withStoreMutation(async () => {
                const current = (await store.getCurrentSsbData())[key];
                assert(
                  current,
                  "installed app exists before its metadata update",
                );
                await store.saveSsbData({
                  ...current,
                  scope: `${app.start_url}updated-scope/`,
                  display: "minimal-ui",
                  theme_color: "#123456",
                });
              }),
              "icon work must not hold the short shared store queue",
            );
            icons.release();
            await within(
              integration,
              "integration did not complete after metadata update",
            );
            const current = (await store.getCurrentSsbData())[key];
            assertEquals(
              current?.scope,
              `${app.start_url}updated-scope/`,
              "prompt save preserves the latest scope",
            );
            assertEquals(
              current?.display,
              "minimal-ui",
              "prompt save preserves the latest display mode",
            );
            assertEquals(
              current?.theme_color,
              "#123456",
              "prompt save preserves the latest theme color",
            );
            assertEquals(
              current?.pinPromptShown,
              true,
              "prompt status is merged into current metadata",
            );
          } finally {
            icons.release();
          }
        }),
    },
    {
      name:
        "rejected app and store mutations do not poison following operations",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const queue = coordinator(fixture.Support);
          for (const kind of ["app", "store"] as const) {
            const enqueue = (operation: () => Promise<string>) =>
              kind === "app"
                ? queue.withMutation(fixture.app.id, operation)
                : queue.withStoreMutation(operation);
            let rejected = false;
            try {
              await enqueue(() =>
                Promise.reject(new Error("Owned injected failure"))
              );
            } catch {
              rejected = true;
            }
            assert(
              rejected,
              "the failed operation remains rejected to its caller",
            );
            assertEquals(
              await within(
                enqueue(() => Promise.resolve("recovered")),
                "queue stayed rejected",
              ),
              "recovered",
              "following operation executes",
            );
          }
        }),
    },

    {
      name:
        "uninstall during native renamed-shortcut completion cleans both names",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store } = fixture;
          const newName = `${app.name}-native-rename`;
          const renamedShortcut = await fixture.ownName(newName);
          const shell = Object.getOwnPropertyDescriptor(
            Support,
            "shellService",
          )!.value as MockShell;
          const descriptor = Object.getOwnPropertyDescriptor(
            shell,
            "createShortcut",
          )!;
          const originalCreate = shell.createShortcut;
          const entered = deferred();
          const release = deferred();
          Object.defineProperty(shell, "createShortcut", {
            ...descriptor,
            value: async (...args: unknown[]) => {
              const result = await originalCreate.apply(shell, args);
              if (args[7] === `${newName}.lnk`) {
                await until(
                  () => IOUtils.exists(renamedShortcut),
                  "native rename did not create its owned shortcut",
                );
                entered.resolve();
                await release.promise;
              }
              return result;
            },
          });
          const rename = fixture.track(
            (new Support() as unknown as StoreAwareSupport).rename(
              { ...app },
              newName,
              store,
            ),
          );
          let removal: Promise<void> | undefined;
          try {
            await within(
              entered.promise,
              "rename did not reach native shortcut completion",
            );
            assert(
              await IOUtils.exists(renamedShortcut),
              "the real native renamed shortcut exists before cancellation",
            );
            removal = fixture.removeLikeManager(new Support());
            release.resolve();
            const [renamed] = await within(
              Promise.all([rename, removal]),
              "native rename cancellation did not drain",
            );
            assertEquals(
              renamed,
              false,
              "the interrupted rename reports cancellation",
            );
            assert(
              !await IOUtils.exists(renamedShortcut),
              "uninstall removes the newly created name absent from the old store record",
            );
            await fixture.assertRemoved();
            await recordWindowsRaceEvidence({
              event: "native-rename",
              id: app.id,
              oldShortcut: fixture.shortcut,
              renamedShortcut,
              renamed,
              pinCalls: [...fixture.pinCalls],
            });
            console.info(
              "[WindowsRace:native-rename] " +
                JSON.stringify({
                  id: app.id,
                  oldShortcut: fixture.shortcut,
                  renamedShortcut,
                  renamed,
                  pinCalls: fixture.pinCalls,
                }),
            );
          } finally {
            release.resolve();
            await Promise.allSettled(removal ? [rename, removal] : [rename]);
            Object.defineProperty(shell, "createShortcut", descriptor);
          }
        }),
    },
    {
      name:
        "uninstall waits for a pin request already handed to the native API",
      fn: () =>
        withWindowsRaceFixture(async (fixture) => {
          const { Support, app, store, key } = fixture;
          const shell = Object.getOwnPropertyDescriptor(
            Support,
            "shellService",
          )!.value as MockShell;
          const pinDescriptor = Object.getOwnPropertyDescriptor(
            shell,
            "pinShortcutToTaskbar",
          )!;
          const unpinDescriptor = Object.getOwnPropertyDescriptor(
            shell,
            "unpinShortcutFromTaskbar",
          )!;
          const originalPin = shell.pinShortcutToTaskbar;
          const originalUnpin = shell.unpinShortcutFromTaskbar;
          const entered = deferred();
          const release = deferred();
          const events: string[] = [];
          Object.defineProperty(shell, "pinShortcutToTaskbar", {
            ...pinDescriptor,
            value: async (...args: unknown[]) => {
              events.push("pin-requested");
              entered.resolve();
              await release.promise;
              await originalPin.apply(shell, args);
              events.push("pin-completed");
            },
          });
          Object.defineProperty(shell, "unpinShortcutFromTaskbar", {
            ...unpinDescriptor,
            value: (...args: unknown[]) => {
              events.push("unpin-requested");
              originalUnpin.apply(shell, args);
            },
          });
          const integration = fixture.track(
            new Support().applyOSIntegration(
              { ...app },
              globalThis as unknown as Window,
            ),
          );
          let removal: Promise<void> | undefined;
          try {
            await within(
              entered.promise,
              "launch did not reach the native pin request",
            );
            removal = fixture.removeLikeManager(new Support());
            await Promise.resolve();
            assertEquals(
              fixture.unpinCalls.length,
              0,
              "removal does not overtake a still-pending native pin completion",
            );
            release.resolve();
            await within(
              Promise.all([integration, removal]),
              "native pin cancellation did not drain",
            );
            assertEquals(
              events.join(","),
              "pin-requested,pin-completed,unpin-requested",
              "native completion precedes removal",
            );
            assert(
              !await IOUtils.exists(fixture.shortcut),
              "the native completion leaves no shortcut after removal",
            );
            assert(
              !await IOUtils.exists(fixture.directory),
              "the native completion leaves no icon directory after removal",
            );
            assert(
              !(key in await store.getCurrentSsbData()),
              "the native completion leaves no installed entry after removal",
            );
            assertEquals(
              fixture.pinCalls.length,
              1,
              "only the request issued before uninstall completes",
            );
            await recordWindowsRaceEvidence({
              event: "native-pin",
              id: app.id,
              shortcut: fixture.shortcut,
              directory: fixture.directory,
              events,
              pinCalls: [...fixture.pinCalls],
              unpinCalls: [...fixture.unpinCalls],
            });
            console.info(
              "[WindowsRace:native-pin] " +
                JSON.stringify({
                  id: app.id,
                  shortcut: fixture.shortcut,
                  directory: fixture.directory,
                  events,
                  pinCalls: fixture.pinCalls,
                  unpinCalls: fixture.unpinCalls,
                }),
            );
          } finally {
            release.resolve();
            await Promise.allSettled(
              removal ? [integration, removal] : [integration],
            );
            Object.defineProperty(shell, "pinShortcutToTaskbar", pinDescriptor);
            Object.defineProperty(
              shell,
              "unpinShortcutFromTaskbar",
              unpinDescriptor,
            );
          }
        }),
    },
  ]);
}
