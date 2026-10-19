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
        "a container badge rendering failure rejects and restores a refresh",
      fn: () =>
        withSandbox(async (root, support) => {
          const { ImageTools } = ChromeUtils.importESModule(
            "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
          );
          const { Experiments } = ChromeUtils.importESModule(
            "resource://noraneko/modules/experiments/Experiments.sys.mjs",
          );
          const { ContextualIdentityService } = ChromeUtils.importESModule(
            "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
          );
          const overrides = [
            { target: Experiments, key: "getVariant", value: () => "enabled" },
            {
              target: ContextualIdentityService,
              key: "getPublicIdentities",
              value: () => [{ userContextId: 901, color: "blue" }],
            },
            {
              target: ImageTools,
              key: "scaleImage",
              value: () => Promise.reject(new Error("Injected badge failure")),
            },
          ].map((override) => ({
            ...override,
            descriptor: Object.getOwnPropertyDescriptor(
              override.target,
              override.key,
            ),
          }));
          const files = paths(root);
          await support.install(manifest);
          const original = await IOUtils.readUTF8(files.desktop);
          try {
            for (const override of overrides) {
              Object.defineProperty(override.target, override.key, {
                configurable: true,
                writable: true,
                value: override.value,
              });
            }
            const { container } = await ImageTools.loadImage(
              Services.io.newURI(manifest.icon),
            );
            assertEquals(
              await ImageTools.applyContainerBadgeToIcon(container, 901),
              container,
              "Default callers retain the original fallback behavior",
            );
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
                { ...manifest, userContextId: 901 },
              );
            } catch {
              rejected = true;
            }
            assert(rejected, "Badge failure rejects the refresh");
            assert(!storeCalled, "Badge failure leaves the store untouched");
            assertEquals(
              await IOUtils.readUTF8(files.desktop),
              original,
              "Old launcher is restored",
            );
            assert(await IOUtils.exists(files.svg), "Old SVG survives");
          } finally {
            for (const override of overrides) {
              if (override.descriptor) {
                Object.defineProperty(
                  override.target,
                  override.key,
                  override.descriptor,
                );
              } else {
                Reflect.deleteProperty(override.target, override.key);
              }
            }
          }
        }),
    },
    {
      name:
        "install and uninstall preserve another profile's destination launcher",
      fn: () =>
        withSandbox(async (root, support) => {
          const files = paths(root);
          const legacy = paths(root, true);
          await support.install(manifest);
          const originalEntry = await IOUtils.readUTF8(files.desktop);
          const foreignEntry = originalEntry.replace(
            "Exec=",
            "Exec=another-profile ",
          );
          await IOUtils.writeUTF8(files.desktop, foreignEntry);
          const icon = await IOUtils.readUTF8(files.svg);
          await IOUtils.makeDirectory(PathUtils.parent(legacy.desktop)!, {
            createAncestors: true,
          });
          await IOUtils.makeDirectory(PathUtils.parent(legacy.svg)!, {
            createAncestors: true,
          });
          const preservedFiles = [
            [files.desktop, foreignEntry],
            [files.svg, icon],
            [files.png, "foreign current PNG"],
            // Even an owned legacy entry must survive an unverified current
            // launcher: uninstall must return before any legacy cleanup.
            [legacy.desktop, originalEntry],
            [legacy.svg, icon],
            [legacy.png, "foreign legacy PNG"],
          ];
          for (const [path, content] of preservedFiles) {
            await IOUtils.writeUTF8(path, content);
          }
          for (
            const { mutate, shouldReject } of [
              {
                mutate: () => support.install({ ...manifest, name: "Changed" }),
                shouldReject: true,
              },
              {
                mutate: () => support.uninstall(manifest),
                shouldReject: false,
              },
            ]
          ) {
            let rejected = false;
            try {
              await mutate();
            } catch {
              rejected = true;
            }
            assertEquals(
              rejected,
              shouldReject,
              "Install rejects foreign ownership; uninstall permits store removal",
            );
            for (const [path, content] of preservedFiles) {
              assertEquals(
                await IOUtils.readUTF8(path),
                content,
                `Current and legacy assets remain unchanged: ${path}`,
              );
            }
          }
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
