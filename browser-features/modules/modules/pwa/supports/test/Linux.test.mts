// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { LinuxSupport } from "../Linux.sys.mts";
import type { Manifest } from "../../type.ts";
import { refreshLauncherForStoreMove } from "#libs/pwa/launcherUpdate.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";

const manifest: Manifest = {
  id: "linux-launcher-regression",
  name: "Linux Launcher",
  start_url: "https://example.com/",
  icon: "data:image/svg+xml," + encodeURIComponent(
    '<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><rect width="32" height="32" fill="purple"/></svg>',
  ),
};

async function withSandbox(
  test: (root: string, support: LinuxSupport) => Promise<void>,
): Promise<void> {
  const root = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "floorp-linux-launcher-test",
  );
  const home = Object.getOwnPropertyDescriptor(LinuxSupport, "cachedHomeDir")!;
  const oldDataHome = Services.env.get("XDG_DATA_HOME");
  try {
    Object.defineProperty(LinuxSupport, "cachedHomeDir", {
      ...home,
      value: PathUtils.join(root, "home"),
    });
    Services.env.set("XDG_DATA_HOME", PathUtils.join(root, "data 日本語"));
    await test(root, new LinuxSupport());
  } finally {
    Object.defineProperty(LinuxSupport, "cachedHomeDir", home);
    Services.env.set("XDG_DATA_HOME", oldDataHome);
    await IOUtils.remove(root, { recursive: true });
  }
}

function paths(root: string, legacy = false) {
  const dataHome = legacy
    ? PathUtils.join(root, "home", ".local", "share")
    : PathUtils.join(root, "data 日本語");
  const basename = "floorp-linux-launcher-regression";
  const iconDirectory = PathUtils.join(
    dataHome,
    "icons",
    "hicolor",
    "512x512",
    "apps",
  );
  return {
    desktop: PathUtils.join(dataHome, "applications", `${basename}.desktop`),
    svg: PathUtils.join(iconDirectory, `${basename}.svg`),
    png: PathUtils.join(iconDirectory, `${basename}.png`),
  };
}

export async function runAllTests(): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  if (AppConstants.platform !== "linux") {
    console.info("[Linux.test] Linux launcher tests require Linux; skipped");
    return;
  }

  await runTests("Linux.test.mts", [
    {
      name:
        "SVG install, rejected store update, and uninstall preserve the launcher",
      fn: () =>
        withSandbox(async (root, support) => {
          const files = paths(root);
          await support.install(manifest);
          const original = await IOUtils.readUTF8(files.desktop);
          assert(
            original.includes(".svg\n"),
            "SVG is referenced by its actual extension",
          );
          assert(await IOUtils.exists(files.svg), "SVG is saved");
          assert(
            !await IOUtils.exists(files.png),
            "No empty PNG is left behind",
          );

          const moved = await refreshLauncherForStoreMove(
            support,
            { moveSsbKey: () => Promise.resolve(false) },
            manifest.start_url,
            manifest,
            { ...manifest, name: "Changed" },
          );
          assertEquals(moved, false, "Store collision is reported");
          assertEquals(
            await IOUtils.readUTF8(files.desktop),
            original,
            "Old launcher is restored",
          );

          await support.uninstall(manifest);
          assert(!await IOUtils.exists(files.desktop), "Launcher is removed");
          assert(!await IOUtils.exists(files.svg), "SVG is removed");
          assert(!await IOUtils.exists(files.png), "PNG is absent");
        }),
    },
    {
      name: "invalid icons allow an initial fallback but reject a refresh",
      fn: () =>
        withSandbox(async (root, support) => {
          const files = paths(root);
          const invalid = { ...manifest, icon: "data:image/png,invalid" };
          await support.install(invalid);
          assert(
            (await IOUtils.readUTF8(files.desktop)).includes("Icon=floorp\n"),
            "Initial install uses the fallback icon",
          );
          await support.install(manifest);
          const original = await IOUtils.readUTF8(files.desktop);
          let rejected = false;
          let storeCalled = false;
          try {
            await refreshLauncherForStoreMove(
              support,
              {
                moveSsbKey: () => {
                  storeCalled = true;
                  return Promise.resolve(true);
                },
              },
              manifest.start_url,
              manifest,
              invalid,
            );
          } catch {
            rejected = true;
          }
          assert(rejected, "Invalid replacement icon fails the refresh");
          assert(!storeCalled, "The store is not changed on icon failure");
          assertEquals(
            await IOUtils.readUTF8(files.desktop),
            original,
            "Old launcher stays usable",
          );
          assert(await IOUtils.exists(files.svg), "Old icon stays present");
        }),
    },
    {
      name:
        "XDG migration removes only a launcher owned by this app and profile",
      fn: () =>
        withSandbox(async (root, support) => {
          const current = paths(root);
          const legacy = paths(root, true);
          Services.env.set("XDG_DATA_HOME", "");
          await support.install(manifest);
          const original = await IOUtils.readUTF8(legacy.desktop);
          Services.env.set(
            "XDG_DATA_HOME",
            PathUtils.join(root, "data 日本語"),
          );
          await IOUtils.writeUTF8(
            legacy.desktop,
            original.replace("Exec=", "Exec=another-profile "),
          );
          await support.install(manifest);
          assert(
            await IOUtils.exists(legacy.desktop),
            "An unrelated profile's launcher is retained",
          );
          await IOUtils.writeUTF8(legacy.desktop, original);
          await support.install(manifest);
          assert(
            await IOUtils.exists(current.desktop),
            "New XDG launcher exists",
          );
          assert(
            !await IOUtils.exists(legacy.desktop),
            "Owned legacy launcher is migrated",
          );
          assert(
            !await IOUtils.exists(legacy.svg),
            "Owned legacy icon is removed",
          );
        }),
    },
  ]);
}
