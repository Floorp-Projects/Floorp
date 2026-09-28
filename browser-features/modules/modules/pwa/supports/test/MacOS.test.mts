// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getMacAppBundle,
  type MacAppOptions,
  MacOSSupport,
  pngToIcns,
} from "../MacOS.sys.mts";
import type { Manifest } from "../../type.ts";
import type { MacAppShimCapabilities } from "#libs/pwa/appRegistryTypes.ts";
import { DataManager } from "../../../../../chrome/common/pwa/dataStore.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";

const ssb: Manifest = {
  id: "{12345678-1234-5678-1234-567812345678}",
  name: "A & B <App> / 日本語",
  start_url: "https://example.com/",
  icon: "",
};

class TestMacOSSupport extends MacOSSupport {
  iconWrites = 0;
  protected override createIcon(): Promise<Uint8Array> {
    this.iconWrites++;
    return Promise.resolve(pngToIcns(new Uint8Array([1, 2, 3])));
  }

  renderIcon(manifest: Manifest): Promise<Uint8Array> {
    return super.createIcon(manifest);
  }
}

function deferred() {
  let resolve!: () => void;
  const promise = new Promise<void>((done) => {
    resolve = done;
  });
  return { promise, resolve };
}

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function timeout<T>(
  promise: Promise<T>,
  milliseconds = 5000,
): Promise<T> {
  let timer: number | undefined;
  try {
    return await Promise.race([
      promise,
      new Promise<never>((_, reject) => {
        timer = setTimeout(
          () => reject(new Error("PWA test timed out")),
          milliseconds,
        );
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}

// Exercise the real data lifecycle in a disposable store, never the user's file.
class TestStore extends DataManager {
  afterSave: (() => Promise<void>) | undefined;
  afterRemove: (() => Promise<void>) | undefined;
  constructor(file: string) {
    super();
    Object.defineProperties(this, {
      ssbStoreFile: { value: file },
      ssbStoreDirectory: { value: PathUtils.parent(file) },
    });
  }
  override async saveSsbData(app: Manifest): Promise<void> {
    await super.saveSsbData(app);
    await this.afterSave?.();
  }
  override async removeSsbData(key: string): Promise<void> {
    await super.removeSsbData(key);
    await this.afterRemove?.();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("MacOS.test.mts", [
    ...(["Contents", "Contents/Resources"] as const).map((markerDirectory) => ({
      name:
        `legacy install and repair preserve ${markerDirectory} native metadata with the feature disabled`,
      async fn() {
        const pref = "floorp.browser.nativeApp.appShim.enabled";
        const hadValue = Services.prefs.prefHasUserValue(pref);
        const oldValue = Services.prefs.getBoolPref(pref, false);
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-native-marker-${crypto.randomUUID()}`,
        );
        const options: MacAppOptions = {
          applicationsDir: PathUtils.join(root, "Applications"),
          profileDir: PathUtils.join(root, "profile"),
          executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
        };
        const support = new TestMacOSSupport(options);
        const bundle = await getMacAppBundle(ssb, options);
        const renamed = { ...ssb, name: "Renamed Native App" };
        const renamedBundle = await getMacAppBundle(renamed, options);
        const store = new TestStore(
          PathUtils.join(root, "profile", "ssb", "ssb.json"),
        );
        try {
          Services.prefs.setBoolPref(pref, false);
          await support.install(ssb, store);
          const markerPath = PathUtils.join(
            bundle.path,
            ...markerDirectory.split("/"),
            "floorp.json",
          );
          if (markerDirectory === "Contents/Resources") {
            await IOUtils.remove(
              PathUtils.join(bundle.path, "Contents", "floorp.json"),
            );
          }
          await IOUtils.writeJSON(markerPath, {
            version: 4,
            integration: "app-shim",
            profileDir: options.profileDir,
            ssb,
          });
          const marker = await IOUtils.readUTF8(markerPath);
          const plistPath = PathUtils.join(
            bundle.path,
            "Contents",
            "Info.plist",
          );
          await IOUtils.writeUTF8(plistPath, "signed-native-sentinel");
          await store.saveSsbData(renamed);
          assertEquals(
            await support.findNativeAppBundle(renamed),
            bundle.path,
            "native discovery preserves identity after rename without a registry",
          );
          await support.install(renamed, store);
          await support.repair(renamed, store);
          assertEquals(
            await IOUtils.readUTF8(markerPath),
            marker,
            "native marker is untouched",
          );
          assertEquals(
            await IOUtils.readUTF8(plistPath),
            "signed-native-sentinel",
            "legacy repair never invalidates the native signature",
          );
          assert(
            !await IOUtils.exists(renamedBundle.path),
            "rename does not create a duplicate shell launcher",
          );
          assertEquals(
            support.iconWrites,
            1,
            "native app repair does not rewrite resources",
          );
        } finally {
          try {
            await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
          } finally {
            if (hadValue) Services.prefs.setBoolPref(pref, oldValue);
            else Services.prefs.clearUserPref(pref);
          }
        }
      },
    })),
    {
      name:
        "missing native capabilities leave launchers and profiles untouched",
      async fn() {
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-mac-capability-${crypto.randomUUID()}`,
        );
        const support = new TestMacOSSupport({
          applicationsDir: PathUtils.join(root, "Applications"),
          profileDir: PathUtils.join(root, "profile"),
          executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
        });
        assertEquals(
          await support.prepareAppShimRegistration(ssb, null),
          null,
          "missing native service keeps legacy integration",
        );
        assertEquals(
          await support.prepareAppShimRegistration(ssb, {
            protocolVersion: 1,
            authenticatedTransport: true,
            nativeWindowOwnership: false,
            sharedBrowserProfile: true,
            transactionalInstall: true,
          }),
          null,
          "partial native support keeps legacy integration",
        );
        assert(
          !await IOUtils.exists(root),
          "unsupported Runtime performs no I/O",
        );
      },
    },
    {
      name: "explicit shim enrollment preserves legacy identity and user data",
      async fn() {
        // The registry binds normalized macOS paths. Other hosts still run the
        // capability fallback and portable legacy launcher tests below.
        if (Services.appinfo.OS !== "Darwin") return;
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-mac-registry-${crypto.randomUUID()}`,
        );
        const options: MacAppOptions = {
          applicationsDir: PathUtils.join(root, "Applications"),
          profileDir: PathUtils.join(root, "profile"),
          executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
        };
        const capabilities: MacAppShimCapabilities = {
          protocolVersion: 1,
          authenticatedTransport: true,
          nativeWindowOwnership: true,
          sharedBrowserProfile: true,
          transactionalInstall: true,
        };
        const support = new TestMacOSSupport(options);
        const app = { ...ssb, name: "既存 App", userContextId: 3 };
        const bundle = await getMacAppBundle(app, options);
        const metadataDirectory = PathUtils.join(options.profileDir, "ssb");
        const legacyStore = PathUtils.join(metadataDirectory, "ssb.json");
        const registryFile = PathUtils.join(
          metadataDirectory,
          "app-registry.json",
        );
        const legacyData = JSON.stringify({ existing: app });
        try {
          await IOUtils.makeDirectory(metadataDirectory, {
            createAncestors: true,
          });
          await IOUtils.writeUTF8(legacyStore, legacyData);
          await support.install(app);
          const launcherFile = PathUtils.join(
            bundle.path,
            "Contents",
            "MacOS",
            "launcher",
          );
          const originalLauncher = await IOUtils.readUTF8(launcherFile);
          const state = await support.prepareAppShimRegistration(
            app,
            capabilities,
          );
          assert(
            state !== null,
            "native coordinator can enroll an owned bundle",
          );
          assertEquals(state!.apps[0].installId, app.id, "keeps SSB ID");
          assertEquals(
            state!.apps[0].bundleId,
            bundle.bundleId,
            "keeps Dock identity",
          );
          assertEquals(
            state!.apps[0].bundlePath,
            bundle.path,
            "keeps existing path",
          );
          assertEquals(
            state!.apps[0].userContextId,
            3,
            "keeps container identity",
          );
          assertEquals(
            state!.apps[0].integration,
            "launcher",
            "enrollment is not a native migration",
          );
          assertEquals(
            await IOUtils.readUTF8(legacyStore),
            legacyData,
            "leaves SSB data untouched",
          );
          assertEquals(
            await IOUtils.readUTF8(launcherFile),
            originalLauncher,
            "leaves working launcher untouched",
          );
          const repeated = await support.prepareAppShimRegistration(
            app,
            capabilities,
          );
          assertEquals(
            repeated!.profile.id,
            state!.profile.id,
            "stable profile identity",
          );
          assertEquals(
            repeated!.revision,
            1,
            "unchanged metadata does not rewrite",
          );

          // A damaged optional registry must neither be silently reset nor
          // prevent the established install/repair path from continuing.
          await IOUtils.writeUTF8(registryFile, "null");
          let rejected = false;
          try {
            await support.prepareAppShimRegistration(app, capabilities);
          } catch {
            rejected = true;
          }
          assert(
            rejected,
            "corrupt registry fails closed for native enrollment",
          );
          await support.install(app);
          assertEquals(
            await IOUtils.readUTF8(registryFile),
            "null",
            "corrupt registry is preserved",
          );
          assertEquals(
            await IOUtils.readUTF8(launcherFile),
            originalLauncher,
            "legacy install remains usable",
          );
          await support.uninstall(app);
          assert(
            !await IOUtils.exists(bundle.path),
            "legacy uninstall remains usable",
          );
        } finally {
          await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
        }
      },
    },
    {
      name:
        "plist filters forbidden XML code points and preserves valid non-BMP text",
      async fn() {
        const valid =
          "\t\n \u0020\ud7ff\ue000\ufffd\u{10000}日本語🌏\u{10ffff}&<>\"'";
        // Construct lone surrogates at runtime so UTF-8 build output preserves
        // the intended UTF-16 code units rather than replacement characters.
        const invalid = "\0\u0001\u0008\u000b\u000c\u000e\u001f\ufffe\uffff" +
          String.fromCharCode(0xd800, 0x58, 0xdfff);
        assertEquals(
          invalid.charCodeAt(invalid.length - 3),
          0xd800,
          "input contains an unpaired high surrogate",
        );
        assertEquals(
          invalid.charCodeAt(invalid.length - 1),
          0xdfff,
          "input contains an unpaired low surrogate",
        );
        const bundle = await getMacAppBundle(
          { ...ssb, name: valid + invalid },
          {
            applicationsDir: PathUtils.tempDir,
            profileDir: "/profile",
            executable: "/floorp",
          },
        );
        const parsed = new DOMParser().parseFromString(
          bundle.plist,
          "text/xml",
        );
        assertEquals(
          parsed.querySelector("parsererror"),
          null,
          "generated plist is well-formed XML",
        );
        const name = Array.from(parsed.querySelectorAll("key")).find((key) =>
          key.textContent === "CFBundleName"
        );
        assertEquals(
          name?.nextElementSibling?.textContent,
          valid + "X",
          "only XML-forbidden code points removed",
        );
      },
    },
    {
      name:
        "bundle metadata escapes XML and shell arguments and isolates profiles",
      async fn() {
        const options: MacAppOptions = {
          applicationsDir: PathUtils.join(PathUtils.tempDir, "Floorp Apps"),
          profileDir: "/Users/O'Brien/Library/Profile $(touch nope)",
          executable: "/Applications/Floorp Nightly.app/Contents/MacOS/floorp",
        };
        const bundle = await getMacAppBundle(ssb, options);
        const parsed = new DOMParser().parseFromString(
          bundle.plist,
          "text/xml",
        );
        assertEquals(
          parsed.querySelector("parsererror"),
          null,
          "valid plist XML",
        );
        const strings = Array.from(
          parsed.querySelectorAll("string"),
          (node) => node.textContent,
        );
        assert(
          strings.includes(ssb.name),
          "display name survives XML encoding",
        );
        assert(
          bundle.launcher.includes("'--start-ssb'"),
          "uses SSB command handler",
        );
        assert(
          bundle.launcher.includes("'--profile'"),
          "selects installing profile",
        );
        assert(
          !bundle.launcher.includes("--new-instance"),
          "keeps profile-specific remoting for an already running PWA",
        );
        assert(
          bundle.launcher.includes(
            "'/usr/bin/open' '-n' '/Applications/Floorp Nightly.app' '--args'",
          ),
          "launches the browser app through LaunchServices with arguments",
        );
        assert(
          bundle.launcher.includes("O'\"'\"'Brien"),
          "quotes apostrophes safely",
        );
        assert(
          bundle.launcher.includes("$(touch nope)'"),
          "substitution stays quoted",
        );
        assertEquals(
          PathUtils.parent(bundle.path),
          options.applicationsDir,
          "safe child path",
        );
        const other = await getMacAppBundle(ssb, {
          ...options,
          profileDir: "/other",
        });
        assert(
          bundle.path !== other.path,
          "copied IDs in different profiles do not collide",
        );
        assertEquals(
          (await getMacAppBundle(ssb, options)).path,
          bundle.path,
          "stable path",
        );
      },
    },
    {
      name:
        "install repairs legacy apps, is idempotent, supports rename and owned removal",
      async fn() {
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-mac-test-${crypto.randomUUID()}`,
        );
        const options: MacAppOptions = {
          applicationsDir: PathUtils.join(root, "Applications"),
          profileDir: PathUtils.join(root, "profile"),
          executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
        };
        const support = new TestMacOSSupport(options);
        // Bundle file operations run on all hosts; XML/name escaping is covered
        // separately above with characters that Windows filenames cannot hold.
        const app = { ...ssb, name: "Portable App 日本語" };
        const { path } = await getMacAppBundle(app, options);
        try {
          // An existing ssb.json entry has no OS files before this installation.
          await Promise.all([support.install(app), support.install(app)]);
          assertEquals(
            support.iconWrites,
            1,
            "concurrent/repeated launch does not rewrite app",
          );
          const contents = PathUtils.join(path, "Contents");
          const launcher = PathUtils.join(contents, "MacOS", "launcher");
          assert(
            (await IOUtils.readUTF8(launcher)).startsWith("#!/bin/sh\nexec "),
            "executable launcher",
          );
          if (Services.appinfo.OS === "Darwin") {
            const quarantine = new TextEncoder().encode(
              "0081;00000000;Floorp;",
            );
            await IOUtils.setMacXAttr(
              path,
              "com.apple.quarantine",
              quarantine,
            );
            await IOUtils.setMacXAttr(
              launcher,
              "com.apple.quarantine",
              quarantine,
            );
            await support.install(app);
            assertEquals(
              support.iconWrites,
              1,
              "quarantine repair does not rewrite a complete bundle",
            );
            assert(
              !await IOUtils.hasMacXAttr(path, "com.apple.quarantine"),
              "complete bundle quarantine is cleared",
            );
            assert(
              !await IOUtils.hasMacXAttr(launcher, "com.apple.quarantine"),
              "complete launcher quarantine is cleared",
            );
          }
          assert(
            await IOUtils.exists(PathUtils.join(contents, "Info.plist")),
            "Finder metadata",
          );
          const icon = PathUtils.join(contents, "Resources", "app.icns");
          const bytes = await IOUtils.read(icon);
          assertEquals(
            new TextDecoder().decode(bytes.slice(0, 4)),
            "icns",
            "icon container",
          );
          assertEquals(
            new DataView(bytes.buffer).getUint32(4),
            bytes.length,
            "ICNS total size",
          );
          await IOUtils.remove(icon);
          await support.install(app);
          assertEquals(support.iconWrites, 2, "partial bundle is repaired");
          const marker = PathUtils.join(contents, "floorp.json");
          const owned = await IOUtils.readUTF8(marker);
          await IOUtils.writeJSON(marker, {
            ssb,
            profileDir: "another-profile",
          });
          let rejected = false;
          try {
            await support.uninstall(app);
          } catch {
            rejected = true;
          }
          assert(rejected, "refuses to remove another profile's bundle");
          assert(await IOUtils.exists(path), "unowned bundle remains");
          await IOUtils.writeUTF8(marker, owned);
          await support.uninstall(app);
          const renamed = { ...app, name: "Renamed" };
          await support.install(renamed);
          assert(!await IOUtils.exists(path), "old bundle removed on rename");
          const renamedPath = (await getMacAppBundle(renamed, options)).path;
          assert(await IOUtils.exists(renamedPath), "renamed bundle created");
          await support.uninstall(renamed);
          await support.uninstall(renamed);
          assert(!await IOUtils.exists(renamedPath), "uninstall is idempotent");
        } finally {
          await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
        }
      },
    },
    {
      name: "install upgrades v1 launchers to Finder-safe bundles",
      async fn() {
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-mac-v1-${crypto.randomUUID()}`,
        );
        const options: MacAppOptions = {
          applicationsDir: PathUtils.join(root, "Applications"),
          profileDir: PathUtils.join(root, "profile"),
          executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
        };
        const support = new TestMacOSSupport(options);
        const app = { ...ssb, name: "Legacy Launcher" };
        const { path } = await getMacAppBundle(app, options);
        const contents = PathUtils.join(path, "Contents");
        const marker = PathUtils.join(contents, "floorp.json");
        const launcher = PathUtils.join(contents, "MacOS", "launcher");
        try {
          await support.install(app);
          const markerData = await IOUtils.readJSON(marker) as {
            version: number;
          };
          await IOUtils.writeJSON(marker, { ...markerData, version: 1 });
          await IOUtils.writeUTF8(
            launcher,
            "#!/bin/sh\nexec '/Applications/Floorp.app/Contents/MacOS/floorp' '--profile' '/wrong'\n",
          );

          await support.install(app);

          assertEquals(
            (await IOUtils.readJSON(marker) as { version: number }).version,
            3,
            "bundle marker records the launcher contract upgrade",
          );
          assert(
            (await IOUtils.readUTF8(launcher)).includes(
              `'--profile' '${options.profileDir}'`,
            ),
            "repair restores the profile-specific launcher",
          );
          assert(
            !(await IOUtils.readUTF8(launcher)).includes("--new-instance"),
            "repair preserves remoting to an already running PWA profile",
          );
          assert(
            (await IOUtils.readUTF8(launcher)).includes(
              "'/usr/bin/open' '-n' '/Applications/Floorp.app' '--args'",
            ),
            "repair replaces direct Mach-O execution with LaunchServices",
          );
        } finally {
          await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
        }
      },
    },
    {
      name:
        "install waits for uninstall ownership validation and recursive removal",
      async fn() {
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-mac-queue-${crypto.randomUUID()}`,
        );
        const options = {
          applicationsDir: root,
          profileDir: "/profile",
          executable: "/floorp",
        };
        const app = { ...ssb, name: "Queue App" };
        const support = new TestMacOSSupport(options);
        const { path } = await getMacAppBundle(app, options);
        const marker = PathUtils.join(path, "Contents", "floorp.json");
        const entered = deferred();
        const release = deferred();
        const originalRead = IOUtils.readJSON;
        const originalRemove = IOUtils.remove;
        const events: string[] = [];
        let recordedOwnershipRead = false;
        const operations: Promise<void>[] = [];
        try {
          await support.install(app);
          IOUtils.readJSON = async (file, opts) => {
            const data = await originalRead.call(IOUtils, file, opts);
            if (file === marker && !recordedOwnershipRead) {
              recordedOwnershipRead = true;
              events.push("validate");
              entered.resolve();
              await release.promise;
            }
            return data;
          };
          IOUtils.remove = async (file, opts) => {
            await originalRemove.call(IOUtils, file, opts);
            if (file === path) events.push("removed");
          };
          const removal = support.uninstall(app);
          operations.push(removal);
          await timeout(entered.promise);
          let installed = false;
          const installation = new TestMacOSSupport(options).install({
            ...app,
            icon: "updated",
          })
            .then(() => {
              installed = true;
              events.push("installed");
            });
          operations.push(installation);
          await new Promise<void>((resolve) => setTimeout(resolve, 100));
          assertEquals(
            installed,
            false,
            "install remains blocked during ownership validation",
          );
          release.resolve();
          await timeout(Promise.all(operations));
          assertEquals(
            events.join(","),
            "validate,removed,installed",
            "whole uninstall precedes subsequent install",
          );
          assert(
            await IOUtils.exists(marker),
            "later install leaves a complete bundle",
          );
        } finally {
          release.resolve();
          await Promise.allSettled(operations);
          IOUtils.readJSON = originalRead;
          IOUtils.remove = originalRemove;
          await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
        }
      },
    },
    {
      name:
        "store writes share the queue and stale launch repair cannot resurrect removed or renamed apps",
      async fn() {
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-mac-store-${crypto.randomUUID()}`,
        );
        await IOUtils.makeDirectory(root);
        const options = {
          applicationsDir: root,
          profileDir: "/profile",
          executable: "/floorp",
        };
        const support = new TestMacOSSupport(options);
        const store = new TestStore(PathUtils.join(root, "ssb.json"));
        const app = { ...ssb, name: "Store App" };
        const { path } = await getMacAppBundle(app, options);
        const saved = deferred();
        const finishSave = deferred();
        const removed = deferred();
        const finishRemove = deferred();
        const operations: Promise<void>[] = [];
        try {
          store.afterSave = async () => {
            saved.resolve();
            await finishSave.promise;
          };
          operations.push(support.install(app, store));
          await timeout(saved.promise);
          operations.push(support.uninstall(app, store));
          await new Promise<void>((resolve) => setTimeout(resolve, 100));
          assert(
            await IOUtils.exists(path),
            "uninstall waits until save has completed",
          );
          finishSave.resolve();
          await timeout(Promise.all(operations));
          assertEquals(
            Object.keys(await store.getCurrentSsbData()).length,
            0,
            "uninstall removes stored app",
          );
          assert(
            !await IOUtils.exists(path),
            "install followed by uninstall leaves no bundle",
          );
          await support.install(app, store);
          store.afterRemove = async () => {
            removed.resolve();
            await finishRemove.promise;
          };
          operations.push(support.uninstall(app, store));
          await timeout(removed.promise);
          operations.push(support.repair(app, store));
          finishRemove.resolve();
          await timeout(Promise.all(operations));
          assert(
            !await IOUtils.exists(path),
            "stale launch does not recreate removed bundle",
          );
          assertEquals(
            Object.keys(await store.getCurrentSsbData()).length,
            0,
            "stale launch does not recreate stored app",
          );
          await support.install(app, store);
          await support.uninstall(app, store);
          const renamed = { ...app, name: "Renamed Store App" };
          await support.install(renamed, store);
          await support.repair(app, store);
          assert(
            !await IOUtils.exists(path),
            "stale launch does not recreate pre-rename bundle",
          );
          const current = await store.getCurrentSsbData();
          assertEquals(
            Object.values(current)[0].name,
            renamed.name,
            "renamed app remains registered",
          );
        } finally {
          finishSave.resolve();
          finishRemove.resolve();
          await Promise.allSettled(operations);
          await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
        }
      },
    },
    {
      name: "missing and invalid site icons produce a valid 512px PNG in ICNS",
      async fn() {
        for (const icon of ["", "data:image/png;base64,invalid"]) {
          const bytes = await new TestMacOSSupport().renderIcon({
            ...ssb,
            icon,
          });
          assertEquals(
            new TextDecoder().decode(bytes.slice(8, 12)),
            "ic09",
            "512px ICNS element",
          );
          const png = bytes.slice(16);
          assertEquals(
            new TextDecoder().decode(png.slice(1, 4)),
            "PNG",
            "PNG signature",
          );
          const view = new DataView(png.buffer);
          assertEquals(view.getUint32(16), 512, "PNG width");
          assertEquals(view.getUint32(20), 512, "PNG height");
        }
      },
    },
  ]);
}
