// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { MacAppShimInstaller } from "../MacAppShimInstaller.sys.mts";
import { AppRegistry } from "../AppRegistry.sys.mts";
import { getMacAppBundle, MacOSSupport } from "../supports/MacOS.sys.mts";
import type { Manifest } from "../type.ts";
import { buildSsbKey } from "#libs/pwa/ssbKeyUtils.ts";
import type { NativeAppService } from "#libs/pwa/nativeAppRuntimeTypes.ts";
import type { MacAppShimCapabilities } from "#libs/pwa/appRegistryTypes.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

function uncallable(): never {
  throw new Error(
    "An unsupported Runtime must not receive an installation request",
  );
}

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

function waitForShimEvent(
  appId: string,
  type: string | number,
  action: () => void,
): Promise<void> {
  return new Promise((resolve, reject) => {
    let settled = false;
    const done = (error?: Error) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      Services.obs.removeObserver(observer, "floorp-web-app-shim-event");
      if (error) reject(error);
      else resolve();
    };
    const observer: nsIObserver = {
      observe(_subject, _topic, data) {
        const event = JSON.parse(data!) as {
          appId: string;
          type: string | number;
        };
        if (event.appId === appId && event.type === type) done();
      },
    };
    Services.obs.addObserver(observer, "floorp-web-app-shim-event");
    const timer = setTimeout(
      () => done(new Error(`Native app event ${type} timed out`)),
      30000,
    );
    try {
      action();
    } catch (error) {
      done(error instanceof Error ? error : new Error(String(error)));
    }
  });
}

async function nativeInstallerLifecycle(): Promise<void> {
  if (Services.appinfo.OS !== "Darwin") return;
  const contract = "@floorp.org/mac-web-app-service;1";
  if (!(contract in Cc)) {
    console.info(
      "[MacAppShimInstaller.test] Native lifecycle skipped: Runtime service unavailable",
    );
    return;
  }
  const service = Cc[contract].getService(
    (Ci as unknown as Record<string, nsIID>).nsIMacWebAppService,
  ) as unknown as NativeAppService;
  const capabilities = JSON.parse(
    service.capabilitiesJSON,
  ) as MacAppShimCapabilities;
  assert(
    capabilities.nativeWindowOwnership === true &&
      capabilities.transactionalInstall === true,
    "native service must report implemented capabilities; tests do not override them",
  );
  const pref = "floorp.browser.nativeApp.appShim.enabled";
  const hadValue = Services.prefs.prefHasUserValue(pref);
  const oldValue = Services.prefs.getBoolPref(pref, false);
  const root = PathUtils.join(
    PathUtils.tempDir,
    `floorp-real-shim-${crypto.randomUUID()}`,
  );
  const options = {
    profileDir: PathUtils.profileDir,
    applicationsDir: PathUtils.join(root, "Applications"),
    executable: Services.dirsvc.get("XREExeF", Ci.nsIFile).path,
  };
  const app: Manifest = {
    id: crypto.randomUUID(),
    name: "Native Transaction Test",
    start_url: "https://example.invalid/",
    icon: "",
    userContextId: 0,
  };
  const entries: Record<string, Manifest> = {
    [buildSsbKey(app.start_url, 0)]: app,
  };
  const store = {
    getCurrentSsbData: () => Promise.resolve(structuredClone(entries)),
    saveSsbData: (manifest: Manifest) => {
      entries[buildSsbKey(manifest.start_url, manifest.userContextId ?? 0)] =
        structuredClone(manifest);
      return Promise.resolve();
    },
    removeSsbData: (key: string) => {
      delete entries[key];
      return Promise.resolve();
    },
  };
  const installer = new MacAppShimInstaller(service, {
    profileDirectory: options.profileDir,
    applicationsDirectory: options.applicationsDir,
    browserExecutable: options.executable,
    shimExecutable: PathUtils.join(
      Services.dirsvc.get("GreBinD", Ci.nsIFile).path,
      "floorp-app-shim",
    ),
  });
  const bundle = await getMacAppBundle(app, options);
  let appWindow: Window | null = null;
  let launched = false;
  try {
    Services.prefs.setBoolPref(pref, true);
    const support = new MacOSSupport(options);
    await support.install(app, store);
    const registration = await support.prepareAppShimRegistration(
      app,
      capabilities,
    );
    assert(registration, "real capabilities allow legacy enrollment");
    service.configure(registration!.profile.id);
    const oldFingerprint = service.fingerprintBundle(bundle.path);
    const first = await installer.prepare(app);
    assert(first?.transactionId, "installer prepared a real signed candidate");
    assertEquals(
      service.fingerprintBundle(bundle.path),
      oldFingerprint,
      "preparing leaves the working launcher intact",
    );
    const verifiedCandidate = JSON.parse(
      service.verifyAppBundle(app.id, first!.bundlePath),
    ) as { bundleId: string };
    assertEquals(
      verifiedCandidate.bundleId,
      bundle.bundleId,
      "candidate retains installed bundle identity",
    );
    await installer.rollback(app.id, first!.transactionId);
    assertEquals(
      service.fingerprintBundle(bundle.path),
      oldFingerprint,
      "rollback preserves exact old bundle contents",
    );
    assert(
      !await IOUtils.exists(first!.bundlePath),
      "rollback removes generated signed candidate",
    );

    const pendingRemoval = await installer.prepare(app);
    assert(pendingRemoval?.transactionId, "uninstall fixture is prepared");
    await new MacOSSupport(options).uninstall(app, store);
    assert(
      !await IOUtils.exists(bundle.path) &&
        !await IOUtils.exists(pendingRemoval!.bundlePath),
      "prepared native uninstall restores then removes only the owned launcher",
    );
    assert(
      !(await AppRegistry.getForProfile().read())?.apps.some((entry) =>
        entry.installId === app.id
      ),
      "prepared-to-legacy uninstall completes without queue reentry",
    );
    await new MacOSSupport(options).install(app, store);

    const prepared = await installer.prepare(app);
    assert(prepared?.transactionId, "retry prepares a fresh transaction");
    service.configure(prepared!.profileId);
    service.registerApp(app.id, prepared!.bundlePath);
    await waitForShimEvent(app.id, "connected", () => {
      launched = true;
      service.launchApp(app.id);
    });
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs",
    );
    const args = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );
    args.data = "about:blank";
    await waitForShimEvent(app.id, 111, () => {
      appWindow = service.withWindowContext(app.id, {
        createWindow: () =>
          BrowserWindowTracker.openWindow({
            args,
            features: "width=640,height=480",
            remote: true,
            fission: true,
          }),
      });
    });
    assertEquals(
      service.getAppIdForWindow(appWindow!),
      app.id,
      "native widget owns this window",
    );
    await installer.commit(app.id);
    assertEquals(
      (await installer.prepare(app))?.bundlePath,
      bundle.path,
      "committed app reuses the original visible path",
    );
    assertEquals(
      (await installer.prepare(app))?.reusedInstalled,
      true,
      "repeat launch does not replace the signed application",
    );
    appWindow!.close();
    appWindow = null;
    await waitForShimEvent(
      app.id,
      "disconnected",
      () => service.sendControl(app.id, 40, "{}"),
    );
    launched = false;

    const renamed = {
      ...app,
      name: "Renamed Native Test",
      short_name: "Renamed Native Test",
    };
    assert(
      await installer.rename(app, renamed, store),
      "native metadata rename succeeds after app exits",
    );
    const installed = (await AppRegistry.getForProfile().read())!.apps.find((
      entry,
    ) => entry.installId === app.id)!;
    assertEquals(
      installed.bundlePath,
      bundle.path,
      "rename retains installed path",
    );
    assertEquals(
      installed.bundleId,
      bundle.bundleId,
      "rename retains LaunchServices identity",
    );
    assertEquals(
      installed.userContextId,
      0,
      "rename keeps normal browser login context",
    );
    assertEquals(
      installed.name,
      renamed.name,
      "rename updates persisted metadata",
    );
    assertEquals(
      JSON.parse(service.verifyAppBundle(app.id, bundle.path)).cdHash,
      installed.installedShim?.cdHash,
      "renamed signature matches the committed receipt",
    );
    await installer.uninstall(renamed, store);
    assert(
      !await IOUtils.exists(bundle.path),
      "uninstall removes the owned native bundle",
    );
    assertEquals(
      Object.keys(entries).length,
      0,
      "uninstall removes only this app's canonical entry",
    );
    assert(
      !(await AppRegistry.getForProfile().read())!.apps.some((entry) =>
        entry.installId === app.id
      ),
      "uninstall removes native metadata to permit reinstall",
    );
    await new MacOSSupport(options).install(app, store);
    assert(
      await IOUtils.exists(bundle.path),
      "explicit reinstall creates a working launcher after native uninstall",
    );
    await new MacOSSupport(options).uninstall(app, store);
  } finally {
    try {
      for (const entry of Services.wm.getEnumerator("")) {
        const window = entry as unknown as Window;
        if (!window.closed && service.getAppIdForWindow(window) === app.id) {
          window.close();
        }
      }
      if (launched) {
        await waitForShimEvent(
          app.id,
          "disconnected",
          () => service.cancelLaunch(app.id),
        );
      }
      // These paths are exclusively owned by this UUID fixture. Retain them
      // for diagnosis if its process cannot be confirmed stopped above.
      await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
      await AppRegistry.getForProfile().forgetApp(app.id, bundle.path);
      const digest = new Uint8Array(
        await crypto.subtle.digest(
          "SHA-256",
          new TextEncoder().encode(app.id).buffer as ArrayBuffer,
        ),
      );
      const token = Array.from(
        digest,
        (byte) => byte.toString(16).padStart(2, "0"),
      ).join("");
      const journal = PathUtils.join(
        PathUtils.profileDir,
        "ssb",
        "app-shim-transactions",
        `${token}.json`,
      );
      await IOUtils.remove(journal, { ignoreAbsent: true });
      await IOUtils.remove(`${journal}.tmp`, { ignoreAbsent: true });
    } finally {
      if (hadValue) Services.prefs.setBoolPref(pref, oldValue);
      else Services.prefs.clearUserPref(pref);
    }
  }
}

const incompleteService: NativeAppService = {
  protocolVersion: 1,
  capabilitiesJSON: JSON.stringify({
    protocolVersion: 1,
    authenticatedTransport: true,
    sharedBrowserProfile: true,
    nativeWindowOwnership: false,
    transactionalInstall: false,
  }),
  get hostIdentityJSON() {
    return uncallable();
  },
  configure: uncallable,
  registerApp: uncallable,
  launchApp: uncallable,
  cancelLaunch: uncallable,
  adoptAppBundlePath: uncallable,
  stop: uncallable,
  getAppIdForWindow: uncallable,
  verifyAppBundle: uncallable,
  fingerprintBundle: uncallable,
  moveStagedBundle: uncallable,
  retireAppBundle: uncallable,
  exchangeAppBundles: uncallable,
  removeStagedBundle: uncallable,
  withWindowContext: uncallable,
  sendControl: uncallable,
};

export async function runAllTests(): Promise<void> {
  await runTests("MacAppShimInstaller.test.mts", [{
    name:
      "opt-in with incomplete Runtime does not create bundles or profile metadata",
    async fn() {
      const pref = "floorp.browser.nativeApp.appShim.enabled";
      const hadValue = Services.prefs.prefHasUserValue(pref);
      const oldValue = Services.prefs.getBoolPref(pref, false);
      const root = PathUtils.join(
        PathUtils.tempDir,
        `floorp-shim-disabled-${crypto.randomUUID()}`,
      );
      const installer = new MacAppShimInstaller(incompleteService, {
        profileDirectory: PathUtils.join(root, "profile"),
        applicationsDirectory: PathUtils.join(root, "Applications"),
        browserExecutable: PathUtils.join(
          root,
          "Floorp.app",
          "Contents",
          "MacOS",
          "floorp",
        ),
        shimExecutable: PathUtils.join(root, "floorp-app-shim"),
      });
      try {
        Services.prefs.setBoolPref(pref, true);
        const prepared = await installer.prepare({
          id: "existing-id",
          name: "Existing App",
          start_url: "https://example.com/",
          icon: "",
          userContextId: 0,
        });
        assertEquals(
          prepared,
          null,
          "incomplete native support uses legacy integration",
        );
        assert(
          !await IOUtils.exists(root),
          "no installation or registry side effects",
        );
      } finally {
        if (hadValue) Services.prefs.setBoolPref(pref, oldValue);
        else Services.prefs.clearUserPref(pref);
      }
    },
  }, {
    name:
      "real native installer rollback, presented-window commit, rename, uninstall and reinstall",
    fn: nativeInstallerLifecycle,
  }]);
}
