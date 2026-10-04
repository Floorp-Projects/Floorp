// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { NRPwaManagerParent } from "../NRPwaManagerParent.sys.mts";
import type { DataManager } from "../../modules/pwa/DataStore.sys.mts";
import type { LinuxSupport } from "../../modules/pwa/supports/Linux.sys.mts";
import type { Manifest } from "../../modules/pwa/type.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";

const manifest: Manifest = {
  id: "linux-settings-actor-test",
  name: "Settings Actor Test",
  start_url: "https://example.com/actor-test",
  userContextId: 0,
  icon: "data:image/svg+xml," + encodeURIComponent(
    '<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><rect width="32" height="32" fill="purple"/></svg>',
  ),
};

function override(target: object, key: string, value: unknown): () => void {
  const descriptor = Object.getOwnPropertyDescriptor(target, key);
  Object.defineProperty(target, key, {
    configurable: true,
    writable: true,
    value,
  });
  return () => {
    if (descriptor) Object.defineProperty(target, key, descriptor);
    else Reflect.deleteProperty(target, key);
  };
}

function paths(root: string) {
  const basename = `floorp-${manifest.id}`;
  const data = PathUtils.join(root, "data 日本語");
  const icons = PathUtils.join(data, "icons", "hicolor", "512x512", "apps");
  return {
    store: PathUtils.join(root, "store", "ssb.json"),
    desktop: PathUtils.join(data, "applications", `${basename}.desktop`),
    svg: PathUtils.join(icons, `${basename}.svg`),
    png: PathUtils.join(icons, `${basename}.png`),
  };
}

async function withSandbox(
  test: (
    root: string,
    store: DataManager,
    support: LinuxSupport,
    actor: NRPwaManagerParent,
  ) => Promise<void>,
): Promise<void> {
  // Use the same module instances that the actor imports at runtime. Only the
  // store paths are redirected; key migration and atomic writes remain real.
  const { DataManager, DataStoreProvider } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/DataStore.sys.mjs",
  ) as typeof import("../../modules/pwa/DataStore.sys.mts");
  const { LinuxSupport } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/supports/Linux.sys.mjs",
  ) as typeof import("../../modules/pwa/supports/Linux.sys.mts");
  const { Experiments } = ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  );
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  const root = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "floorp-linux-settings-actor-test",
  );
  const store = new DataManager();
  Object.defineProperties(store, {
    ssbStoreDirectory: { value: PathUtils.join(root, "store") },
    ssbStoreFile: { value: paths(root).store },
  });
  const oldDataHome = Services.env.get("XDG_DATA_HOME");
  const restorers: Array<() => void> = [];
  const originalGetVariant = Experiments.getVariant.bind(Experiments);
  try {
    restorers.push(override(DataStoreProvider, "getDataManager", () => store));
    restorers.push(
      override(LinuxSupport, "cachedHomeDir", PathUtils.join(root, "home")),
    );
    restorers.push(
      override(
        Experiments,
        "getVariant",
        (key: string) =>
          key === "pwa_container_support" ? "enabled" : originalGetVariant(key),
      ),
    );
    restorers.push(
      override(
        ContextualIdentityService,
        "getPublicIdentities",
        () => [{ userContextId: 1, color: "blue" }],
      ),
    );
    restorers.push(
      override(
        ContextualIdentityService,
        "getUserContextLabel",
        () => "Actor Container",
      ),
    );
    Services.env.set("XDG_DATA_HOME", PathUtils.join(root, "data 日本語"));
    const support = new LinuxSupport();
    await store.saveSsbData(manifest);
    await support.install(manifest);
    const actor = Object.create(
      NRPwaManagerParent.prototype,
    ) as NRPwaManagerParent;
    // A regression to the old direct-write implementation must remain confined
    // to this sandbox too; otherwise a failing test could touch the real profile.
    Object.defineProperty(actor, "installedAppsStoreFile", {
      value: paths(root).store,
    });
    await test(root, store, support, actor);
  } finally {
    for (const restore of restorers.reverse()) restore();
    Services.env.set("XDG_DATA_HOME", oldDataHome);
    await IOUtils.remove(root, { recursive: true });
  }
}

function setContainer(
  actor: NRPwaManagerParent,
  userContextId: number,
  id = manifest.id,
) {
  return actor.receiveMessage({
    name: "PwaManager:SetContainer",
    data: { id, userContextId },
  } as ReceiveMessageArgument);
}

async function snapshot(root: string): Promise<string> {
  const files = paths(root);
  const contents = await Promise.all(
    Object.values(files).map(async (file) =>
      await IOUtils.exists(file) ? Array.from(await IOUtils.read(file)) : null
    ),
  );
  return JSON.stringify(contents);
}

export async function runAllTests(): Promise<void> {
  if (Services.appinfo.OS !== "Linux") {
    console.info(
      "[NRPwaManagerParent.test] Linux launcher tests require Linux; skipped",
    );
    return;
  }
  await runTests("NRPwaManagerParent.test.mts", [
    {
      name:
        "settings container changes refresh the launcher and store in both directions",
      fn: () =>
        withSandbox(async (root, store, _support, actor) => {
          const files = paths(root);
          const originalDesktop = await IOUtils.readUTF8(files.desktop);
          assertEquals(
            await setContainer(actor, 1),
            "ok",
            "Container assignment succeeds",
          );
          const assigned = await store.getCurrentSsbData();
          assertEquals(
            assigned[`${manifest.start_url}:1`]?.userContextId,
            1,
            "The new store key has its container",
          );
          assert(
            !assigned[`${manifest.start_url}:0`],
            "The old key is removed",
          );
          const desktop = await IOUtils.readUTF8(files.desktop);
          assert(
            desktop.includes("Name=Settings Actor Test (Actor Container)\n"),
            "The desktop entry has the container label",
          );
          assert(
            desktop.includes(`${files.png}\n`),
            "The desktop entry references the badged PNG",
          );
          assert(await IOUtils.exists(files.png), "The badged PNG exists");
          assert(
            !await IOUtils.exists(files.svg),
            "The obsolete SVG is removed",
          );

          assertEquals(
            await setContainer(actor, 0),
            "ok",
            "Clearing the container succeeds",
          );
          const cleared = await store.getCurrentSsbData();
          assertEquals(
            cleared[`${manifest.start_url}:0`]?.userContextId ?? 0,
            0,
            "The store returns to the default context",
          );
          assert(
            !cleared[`${manifest.start_url}:1`],
            "The container key is removed",
          );
          assertEquals(
            await IOUtils.readUTF8(files.desktop),
            originalDesktop,
            "The original launcher name and SVG are restored",
          );
          assert(await IOUtils.exists(files.svg), "The original SVG exists");
          assert(
            !await IOUtils.exists(files.png),
            "The obsolete badge is removed",
          );
        }),
    },
    {
      name:
        "an occupied container key preserves both apps and the existing launcher",
      fn: () =>
        withSandbox(async (root, store, _support, actor) => {
          await store.saveSsbData({
            ...manifest,
            id: "other-app",
            userContextId: 1,
          });
          const before = await snapshot(root);
          assertEquals(
            await setContainer(actor, 1),
            "container-conflict",
            "An occupied key is rejected",
          );
          assertEquals(
            await snapshot(root),
            before,
            "The store and launcher remain byte-for-byte unchanged",
          );
        }),
    },
    {
      name:
        "strict badge errors and rejected store moves restore the original launcher",
      fn: () =>
        withSandbox(async (root, store, _support, actor) => {
          const { ImageTools } = ChromeUtils.importESModule(
            "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
          );
          const before = await snapshot(root);
          const restoreScale = override(ImageTools, "scaleImage", () =>
            Promise.reject(new Error("Injected badge failure")));
          try {
            assertEquals(
              await setContainer(actor, 1),
              "failed",
              "Badge failure is reported to the settings page",
            );
            assertEquals(
              await snapshot(root),
              before,
              "Badge failure preserves the launcher and store",
            );
          } finally {
            restoreScale();
          }

          let attemptedMove = false;
          const restoreMove = override(store, "moveSsbKey", async () => {
            attemptedMove = true;
            assert(
              await IOUtils.exists(paths(root).png),
              "The replacement icon was written before the store move",
            );
            return false;
          });
          try {
            assertEquals(
              await setContainer(actor, 1),
              "failed",
              "A rejected store move reports failure",
            );
            assert(
              attemptedMove,
              "The launcher refresh reached the store move",
            );
            assertEquals(
              await snapshot(root),
              before,
              "Rollback restores the launcher and removes the replacement icon",
            );
          } finally {
            restoreMove();
          }
        }),
    },
    {
      name:
        "invalid, missing, and disabled mutations do not change stored apps or launchers",
      fn: () =>
        withSandbox(async (root, _store, _support, actor) => {
          const before = await snapshot(root);
          for (
            const value of [-1, 0.5, Number.NaN, Number.MAX_SAFE_INTEGER + 1]
          ) {
            assertEquals(
              await setContainer(actor, value),
              "invalid-container",
              "Invalid container is rejected",
            );
          }
          assertEquals(
            await setContainer(actor, 1, "missing-app"),
            "not-found",
            "A missing app is rejected",
          );
          const { Experiments } = ChromeUtils.importESModule(
            "resource://noraneko/modules/experiments/Experiments.sys.mjs",
          );
          const restoreVariant = override(
            Experiments,
            "getVariant",
            () => null,
          );
          try {
            assertEquals(
              await setContainer(actor, 1),
              "disabled",
              "The experiment gate is enforced",
            );
          } finally {
            restoreVariant();
          }
          assertEquals(
            await snapshot(root),
            before,
            "Rejected requests preserve all files",
          );
        }),
    },
  ]);
}
