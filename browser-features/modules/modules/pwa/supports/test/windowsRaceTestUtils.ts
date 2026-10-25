// SPDX-License-Identifier: MPL-2.0

import type { Manifest } from "../../type.ts";
import type { WindowsSupport } from "../Windows.sys.mts";
import type {
  OwnedWindowsApp,
  WindowsRaceFixture,
  WindowsRaceStore,
} from "./types.ts";
import {
  assert,
  assertEquals,
} from "../../../../../chrome/test/utils/test_harness.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

let evidenceQueue: Promise<void> = Promise.resolve();

function assertCollectorProfile(): void {
  assert(
    /[\\/]_dist[\\/]profile[\\/]test[\\/]?$/i.test(PathUtils.profileDir),
    "Windows PWA tests require the owned _dist/profile/test collector profile",
  );
}

export function recordWindowsRaceEvidence(
  event: Record<string, unknown>,
): Promise<void> {
  assertCollectorProfile();
  const file = PathUtils.join(
    PathUtils.profileDir,
    "windows-pwa-race-evidence.json",
  );
  const result = evidenceQueue.then(async () => {
    const previous: unknown = await IOUtils.exists(file)
      ? await IOUtils.readJSON(file)
      : [];
    assert(
      Array.isArray(previous),
      "owned Windows race evidence is an event array",
    );
    await IOUtils.writeJSON(file, [
      ...previous,
      { timestamp: new Date().toISOString(), ...event },
    ], { tmpPath: `${file}.tmp` });
  });
  evidenceQueue = result.then(() => {}, () => {});
  return result;
}

export function deferred() {
  let resolve!: () => void;
  const promise = new Promise<void>((done) => {
    resolve = done;
  });
  return { promise, resolve };
}

export function delay(milliseconds: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

export async function within<T>(
  promise: Promise<T>,
  label: string,
  milliseconds = 8000,
): Promise<T> {
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    return await Promise.race([
      promise,
      new Promise<never>((_, reject) => {
        timer = setTimeout(() => reject(new Error(label)), milliseconds);
      }),
    ]);
  } finally {
    if (timer !== undefined) clearTimeout(timer);
  }
}

export async function until(
  predicate: () => Promise<boolean>,
  label: string,
): Promise<void> {
  const deadline = Date.now() + 6000;
  while (!(await predicate())) {
    assert(Date.now() < deadline, label);
    await delay(10);
  }
}

/** Real ICO/store/Start Menu files in the collector profile; only pin UI and
 * window decoration are stubbed. createShortcut delegates to the native shell.
 * Cleanup removes only this UUID's exact shortcut, app directory and store key.
 */
export async function withWindowsRaceFixture(
  test: (fixture: WindowsRaceFixture) => Promise<void>,
): Promise<void> {
  assertCollectorProfile();
  const { WindowsSupport: Support } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/supports/Windows.sys.mjs",
  ) as { WindowsSupport: typeof WindowsSupport };
  const { DataStoreProvider } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/DataStore.sys.mjs",
  );
  const { ImageTools } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
  );
  const { buildSsbKey } = await import("#libs/pwa/ssbKeyUtils.ts");
  const uuid = Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
  assert(/^[a-f0-9-]{36}$/i.test(uuid), "fixture UUID is a safe filename");
  const id = `floorp-windows-race-test-${uuid}`;
  const app: Manifest = {
    id,
    name: id,
    start_url: `https://example.invalid/${id}/`,
    userContextId: 0,
    pinPromptShown: false,
    icon: "data:image/svg+xml," + encodeURIComponent(
      '<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><rect width="32" height="32" fill="purple"/></svg>',
    ),
  };
  const directory = PathUtils.join(PathUtils.profileDir, "ssb", id);
  const programs = PathUtils.join(
    Services.dirsvc.get("Home", Ci.nsIFile).path,
    "AppData",
    "Roaming",
    "Microsoft",
    "Windows",
    "Start Menu",
    "Programs",
  );
  const shortcut = PathUtils.join(programs, `${id}.lnk`);
  assertEquals(
    PathUtils.parent(directory),
    PathUtils.join(PathUtils.profileDir, "ssb"),
    "app directory stays in the collector profile",
  );
  assertEquals(
    PathUtils.parent(shortcut),
    programs,
    "shortcut stays in Programs",
  );
  assert(!await IOUtils.exists(directory), "UUID app directory is unoccupied");
  assert(!await IOUtils.exists(shortcut), "UUID shortcut is unoccupied");

  const store: WindowsRaceStore = DataStoreProvider.getDataManager();
  const key = buildSsbKey(app.start_url, 0);
  assert(
    !(key in await store.getCurrentSsbData()),
    "UUID store key is unoccupied",
  );
  const ownedApps: OwnedWindowsApp[] = [{ app, key, directory, shortcut }];
  const ownedShortcuts = new Set([shortcut]);
  const ownedNames = new Set([`${id}.lnk`]);
  const ownName = async (name: string): Promise<string> => {
    assert(
      name.startsWith(`${id}-`) && /^[a-z0-9-]+$/i.test(name),
      "additional shortcut names retain this fixture's safe UUID prefix",
    );
    const path = PathUtils.join(programs, `${name}.lnk`);
    assertEquals(
      PathUtils.parent(path),
      programs,
      "additional shortcut stays in Programs",
    );
    if (!ownedShortcuts.has(path)) {
      assert(
        !await IOUtils.exists(path),
        "additional UUID shortcut is unoccupied",
      );
      ownedShortcuts.add(path);
      ownedNames.add(`${name}.lnk`);
      await recordWindowsRaceEvidence({ event: "owned-shortcut", id, path });
    }
    return path;
  };
  console.info(
    "[WindowsRace:fixture] " + JSON.stringify({
      profile: PathUtils.profileDir,
      id,
      key,
      directory,
      shortcut,
      shortcutWriter: "actual nsIWindowsShellService.createShortcut",
      pinUI: "stubbed",
    }),
  );
  await recordWindowsRaceEvidence({
    event: "fixture",
    profile: PathUtils.profileDir,
    id,
    key,
    directory,
    shortcut,
    shortcutWriter: "actual nsIWindowsShellService.createShortcut",
    pinUI: "stubbed",
  });
  const shellDescriptor = Object.getOwnPropertyDescriptor(
    Support,
    "shellService",
  )!;
  const uiDescriptor = Object.getOwnPropertyDescriptor(Support, "uiUtils")!;
  const taskbarDescriptor = Object.getOwnPropertyDescriptor(
    Support,
    "taskbar",
  )!;
  const loadDescriptor = Object.getOwnPropertyDescriptor(
    ImageTools,
    "loadImage",
  )!;
  const nativeShell = shellDescriptor.value as {
    createShortcut: (...args: unknown[]) => unknown;
  };
  const pinCalls: string[] = [];
  const unpinCalls: string[] = [];
  const operations: Promise<unknown>[] = [];
  const releases: Array<() => void> = [];
  const track = <T>(operation: Promise<T>): Promise<T> => {
    operations.push(operation);
    // The caller awaits the operation after arranging the race.
    void operation.catch(() => {});
    return operation;
  };

  try {
    Object.defineProperty(Support, "shellService", {
      ...shellDescriptor,
      value: {
        createShortcut: (...args: unknown[]) => {
          assertEquals(args[6], "Programs", "native shortcut folder is owned");
          assert(
            typeof args[7] === "string" && ownedNames.has(args[7]),
            "native shortcut filename is explicitly owned",
          );
          return track(Promise.resolve(nativeShell.createShortcut(...args)));
        },
        pinShortcutToTaskbar: (
          _group: string,
          _folder: string,
          name: string,
        ) => {
          pinCalls.push(name);
          return Promise.resolve();
        },
        unpinShortcutFromTaskbar: (_folder: string, name: string) => {
          unpinCalls.push(name);
        },
      },
    });
    Object.defineProperty(Support, "uiUtils", {
      ...uiDescriptor,
      value: {
        systemSmallIconSize: 16,
        systemLargeIconSize: 32,
        setWindowIcon: () => {},
      },
    });
    Object.defineProperty(Support, "taskbar", {
      ...taskbarDescriptor,
      value: { setGroupIdForWindow: () => {} },
    });

    await store.saveSsbData({ ...app });
    await track(new Support().install({ ...app }));
    // The original install forgot to await native createShortcut's promise.
    // Wait on its observable file before starting either regression scenario.
    await until(
      () => IOUtils.exists(shortcut),
      "native fixture shortcut was not created",
    );
    assert(await IOUtils.exists(shortcut), "native fixture shortcut exists");
    assert(
      await IOUtils.exists(directory),
      "native fixture icon directory exists",
    );
    pinCalls.length = 0;
    unpinCalls.length = 0;

    await test({
      Support,
      app,
      key,
      store,
      directory,
      shortcut,
      pinCalls,
      unpinCalls,
      track,
      ownName,
      async ownApp(suffix) {
        assert(
          /^[a-z0-9-]+$/i.test(suffix),
          "extra app suffix is a safe filename",
        );
        const otherId = `${id}-${suffix}`;
        const otherApp: Manifest = {
          ...app,
          id: otherId,
          name: otherId,
          start_url: `https://example.invalid/${otherId}/`,
          icon: "data:image/svg+xml," + encodeURIComponent(
            `<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><title>${otherId}</title><rect width="32" height="32" fill="blue"/></svg>`,
          ),
        };
        const otherDirectory = PathUtils.join(
          PathUtils.profileDir,
          "ssb",
          otherId,
        );
        assertEquals(
          PathUtils.parent(otherDirectory),
          PathUtils.join(PathUtils.profileDir, "ssb"),
          "extra app stays in the collector profile",
        );
        assert(
          !await IOUtils.exists(otherDirectory),
          "extra UUID app directory is unoccupied",
        );
        const otherKey = buildSsbKey(otherApp.start_url, 0);
        assert(
          !(otherKey in await store.getCurrentSsbData()),
          "extra UUID store key is unoccupied",
        );
        const otherShortcut = await ownName(otherId);
        const owned = {
          app: otherApp,
          key: otherKey,
          directory: otherDirectory,
          shortcut: otherShortcut,
        };
        ownedApps.push(owned);
        console.info("[WindowsRace:fixture] " + JSON.stringify(owned));
        await recordWindowsRaceEvidence({
          event: "additional-fixture",
          ...owned,
        });
        return owned;
      },
      pauseIconLoads() {
        const gate = deferred();
        const entered = deferred();
        const originalLoad = loadDescriptor.value as (
          uri: nsIURI,
        ) => Promise<{ type: string; container: imgIContainer }>;
        releases.push(gate.resolve);
        let calls = 0;
        Object.defineProperty(ImageTools, "loadImage", {
          ...loadDescriptor,
          value: async (uri: nsIURI) => {
            if (uri.spec !== app.icon) {
              return await originalLoad.call(ImageTools, uri);
            }
            calls++;
            if (calls === 2) entered.resolve();
            await gate.promise;
            return await originalLoad.call(ImageTools, uri);
          },
        });
        return { entered: entered.promise, release: gate.resolve };
      },
      removeLikeManager(support) {
        // Baseline accepts only one argument and ignores the store; the fixed
        // implementation accepts it. The original manager's removal is always
        // emulated too, so baseline failure reflects the race, not a new API.
        const compatible = support as unknown as {
          uninstall(app: Manifest, store?: WindowsRaceStore): Promise<void>;
        };
        return track(
          compatible.uninstall({ ...app }, store).then(() =>
            store.removeSsbData(key)
          ),
        );
      },
      async assertRemoved() {
        // Baseline does not await native shortcut completion. Observe its final
        // filesystem result rather than racing that promise in the assertion.
        await Promise.allSettled(operations);
        const outcome = {
          id,
          key,
          directory,
          shortcut,
          shortcutExists: await IOUtils.exists(shortcut),
          directoryExists: await IOUtils.exists(directory),
          storeEntryExists: key in await store.getCurrentSsbData(),
          pinCalls: [...pinCalls],
          unpinCalls: [...unpinCalls],
        };
        console.info("[WindowsRace:outcome] " + JSON.stringify(outcome));
        await recordWindowsRaceEvidence({ event: "outcome", ...outcome });
        assert(
          !outcome.shortcutExists,
          "uninstall must not recreate the owned Start Menu shortcut",
        );
        assert(
          !outcome.directoryExists,
          "uninstall must not recreate the owned icon directory",
        );
        assert(
          !outcome.storeEntryExists,
          "late launch integration must not resurrect the store entry",
        );
        assertEquals(
          pinCalls.length,
          0,
          "cancelled launch must not request taskbar pinning",
        );
      },
    });
  } finally {
    for (const release of releases) release();
    // Drain delayed work before restoring real pin methods or deleting fixtures.
    await Promise.allSettled(operations);
    try {
      const beforeCleanup = await store.getCurrentSsbData();
      await recordWindowsRaceEvidence({
        event: "before-cleanup",
        id,
        apps: await Promise.all(ownedApps.map(async (owned) => ({
          id: owned.app.id,
          key: owned.key,
          directory: owned.directory,
          directoryExists: await IOUtils.exists(owned.directory),
          stored: beforeCleanup[owned.key] ?? null,
        }))),
        shortcuts: await Promise.all([...ownedShortcuts].map(async (path) => ({
          path,
          exists: await IOUtils.exists(path),
        }))),
        pinCalls: [...pinCalls],
        unpinCalls: [...unpinCalls],
      });
    } finally {
      Object.defineProperty(ImageTools, "loadImage", loadDescriptor);
      Object.defineProperty(Support, "shellService", shellDescriptor);
      Object.defineProperty(Support, "uiUtils", uiDescriptor);
      Object.defineProperty(Support, "taskbar", taskbarDescriptor);
      for (const owned of ownedApps) {
        await store.removeSsbData(owned.key);
        await IOUtils.remove(owned.directory, {
          recursive: true,
          ignoreAbsent: true,
        });
      }
      for (const ownedShortcut of ownedShortcuts) {
        await IOUtils.remove(ownedShortcut, { ignoreAbsent: true });
      }
      const afterCleanup = await store.getCurrentSsbData();
      await recordWindowsRaceEvidence({
        event: "cleanup",
        id,
        apps: await Promise.all(ownedApps.map(async (owned) => ({
          id: owned.app.id,
          key: owned.key,
          directory: owned.directory,
          directoryExists: await IOUtils.exists(owned.directory),
          storeEntryExists: owned.key in afterCleanup,
        }))),
        shortcuts: await Promise.all([...ownedShortcuts].map(async (path) => ({
          path,
          exists: await IOUtils.exists(path),
        }))),
      });
      console.info(
        "[WindowsRace:cleanup] " + JSON.stringify({
          ids: ownedApps.map((owned) => owned.app.id),
          directories: ownedApps.map((owned) => owned.directory),
          shortcuts: [...ownedShortcuts],
        }),
      );
    }
  }
}
