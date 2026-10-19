// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zipSync } from "fflate/browser";
import type {
  NativeExtension,
  NativeManager,
  PermissionPrompt,
  Reply,
} from "./helpers/update-test-types.ts";
import { CRXConverterClass } from "../CRXConverter.sys.mts";
import type { ChromeConversionReport } from "../types.ts";
import { ChromeExtensionUpdaterClass } from "../ChromeExtensionUpdater.sys.mts";
import { createSignedCRX3, CRX_TEST_ID } from "./helpers/CRXFixtures.mts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

export const ID = `${CRX_TEST_ID}@chromium-extension`;
const encoder = new TextEncoder();
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export function manager(): NativeManager {
  return (
    ChromeUtils.importESModule(
      "resource://gre/modules/AddonManager.sys.mjs",
    ) as unknown as { AddonManager: NativeManager }
  ).AddonManager;
}

export function browser(): Element {
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as {
    gBrowser: { selectedBrowser: Element };
  };
  return win.gBrowser.selectedBrowser;
}

export async function waitFor<T>(
  read: () => T | false | undefined,
  name: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const result = read();
    if (result) return result;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${name}`);
}

function registry(): Map<string, NativeExtension> {
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  return ExtensionParent.GlobalManager.extensionMap as Map<
    string,
    NativeExtension
  >;
}

let request = 0;
export async function command(
  operation: "get" | "set",
  value?: string,
): Promise<Reply> {
  const extension = await waitFor(
    () => registry().get(ID),
    "extension registry",
  );
  const context = await waitFor(
    () => extension.backgroundContext,
    "background context",
  );
  const sequence = ++request;
  const payload = { updaterFixture: true, request: sequence, operation, value };
  context.xulBrowser.messageManager.loadFrameScript(
    `data:application/javascript,${
      encodeURIComponent(
        `content.postMessage(${JSON.stringify(payload)}, '*');`,
      )
    }`,
    false,
  );
  return await waitFor(() => {
    try {
      const reply = JSON.parse(context.xulBrowser.contentTitle) as Reply;
      return reply.request === sequence ? reply : false;
    } catch {
      return false;
    }
  }, "storage response");
}

export async function packageVersion(
  version: string,
  options: {
    permissions?: string[];
    postpone?: boolean;
    impersonate?: boolean;
  } = {},
): Promise<ArrayBuffer> {
  return await createSignedCRX3(
    zipSync({
      "manifest.json": encoder.encode(
        JSON.stringify({
          manifest_version: 3,
          name: "Floorp Chrome update regression fixture",
          version,
          permissions: options.permissions ?? ["storage"],
          optional_permissions: ["alarms"],
          background: { service_worker: "background.js" },
        }),
      ),
      "background.js": encoder.encode(`
      let pending;
      ${
        options.postpone
          ? "browser.runtime.onUpdateAvailable.addListener(update => { pending = update.version; });"
          : ""
      }
      window.addEventListener('message', async event => {
        const data = event.data;
        if (!data || data.updaterFixture !== true) return;
        try {
          if (data.operation === 'set') await browser.storage.local.set({ retained: data.value });
          const stored = await browser.storage.local.get('retained');
          document.title = JSON.stringify({ request: data.request, value: stored.retained,
            id: browser.runtime.id, version: browser.runtime.getManifest().version, pending });
        } catch (error) { document.title = JSON.stringify({ request: data.request, error: String(error) }); }
      });
      document.title = JSON.stringify({ ready: true });
    `),
    }),
    options.impersonate
      ? { algorithm: "ecdsa", declaredChromeId: CRX_TEST_ID }
      : {},
  );
}

export function observePrompts(
  topic:
    | "webextension-permission-prompt"
    | "webextension-update-permission-prompt",
  handle: (prompt: PermissionPrompt) => void,
): () => void {
  const observer = {
    observe(subject: nsISupports) {
      const wrapped = (
        subject as unknown as {
          wrappedJSObject: PermissionPrompt & { info?: PermissionPrompt };
        }
      ).wrappedJSObject;
      const prompt = wrapped.info ?? wrapped;
      if (prompt.addon?.id === ID) handle(prompt);
    },
  };
  Services.obs.addObserver(observer, topic);
  return () => Services.obs.removeObserver(observer, topic);
}

export async function initialInstall(
  crx: ArrayBuffer,
): Promise<{ xpiData: ArrayBuffer; report: ChromeConversionReport }> {
  const converted = await new CRXConverterClass().convert(crx, CRX_TEST_ID, {
    id: CRX_TEST_ID,
    name: "Update fixture",
  });
  assert(
    converted.success && converted.xpiData,
    "initial authenticated conversion",
  );
  const file = Services.dirsvc.get("TmpD", Ci.nsIFile);
  file.append(`floorp-updater-fixture-${Services.uuid.generateUUID()}.xpi`);
  file.createUnique(0, 0o600);
  const stop = observePrompts(
    "webextension-permission-prompt",
    (prompt) => prompt.resolve(),
  );
  try {
    await IOUtils.write(file.path, new Uint8Array(converted.xpiData));
    const install = await manager().getInstallForFile(file);
    assertEquals(
      install.error,
      0,
      "normal persistent install accepts the converted fixture under isolated signature preference",
    );
    await new Promise<void>((resolve, reject) => {
      const finish = (error?: Error) => {
        clearTimeout(timer);
        install.removeListener(listener);
        if (error) reject(error);
        else resolve();
      };
      const listener = {
        onInstallEnded() {
          finish();
        },
        onInstallFailed() {
          finish(
            new Error(`Initial fixture install failed (${install.error})`),
          );
        },
        onInstallCancelled() {
          finish(new Error("Initial fixture install cancelled"));
        },
        onDownloadCancelled() {
          finish(new Error("Initial fixture preparation cancelled"));
        },
      };
      const timer = setTimeout(() => {
        finish(new Error("Initial fixture install timed out"));
        install.cancel();
      }, 15000);
      install.addListener(listener);
      manager().installAddonFromAOMWithOptions(browser(), null, install, {
        preferUpdateOverInstall: false,
      });
    });
    await waitFor(
      () =>
        registry()
          .get(ID)
          ?.backgroundContext?.xulBrowser.contentTitle.includes('"ready":true'),
      "initial background ready",
    );
    const installed = await manager().getAddonByID(ID);
    assert(
      installed && !installed.temporarilyInstalled,
      "fixture uses permanent AddonManager install",
    );
    assert(converted.report, "fixture conversion report");
    return { xpiData: converted.xpiData, report: converted.report };
  } finally {
    stop();
    await IOUtils.remove(file.path, { ignoreAbsent: true });
  }
}

export async function fixture(
  fn: (converted: {
    xpiData: ArrayBuffer;
    report: ChromeConversionReport;
  }) => Promise<void>,
  options: { postpone?: boolean } = {},
): Promise<void> {
  const oldTemporaryFiles = new Set(
    await IOUtils.getChildren(PathUtils.tempDir),
  );
  const hadPref = Services.prefs.prefHasUserValue(
    "xpinstall.signatures.required",
  );
  const signing = Services.prefs.getBoolPref(
    "xpinstall.signatures.required",
    true,
  );
  Services.prefs.setBoolPref("xpinstall.signatures.required", false);
  try {
    const converted = await initialInstall(
      await packageVersion("1.0", options),
    );
    await fn(converted);
  } finally {
    for (const install of await manager().getAllInstalls()) {
      if (
        install.addon?.id === ID &&
        install.state === manager().STATE_POSTPONED
      ) {
        install.cancel();
      }
    }
    const addon = await manager().getAddonByID(ID);
    if (addon) await addon.uninstall();
    if (hadPref) {
      Services.prefs.setBoolPref("xpinstall.signatures.required", signing);
    } else Services.prefs.clearUserPref("xpinstall.signatures.required");
    let leaked: string[] = [];
    const deadline = Date.now() + 3000;
    do {
      leaked = (await IOUtils.getChildren(PathUtils.tempDir)).filter(
        (path) =>
          !oldTemporaryFiles.has(path) &&
          PathUtils.filename(path).startsWith("floorp-chrome-update-"),
      );
      if (!leaked.length) break;
      await new Promise((resolve) => setTimeout(resolve, 25));
    } while (Date.now() < deadline);
    assertEquals(
      leaked.length,
      0,
      "service removes temporary update payload after success, cancellation, or postponement",
    );
  }
}

export async function rejected(
  promise: Promise<unknown>,
  text: string,
): Promise<void> {
  try {
    await promise;
  } catch (error) {
    assert(String(error).includes(text), `failure describes ${text}: ${error}`);
    return;
  }
  throw new Error(`Expected rejection describing ${text}`);
}

const tests: TestCase[] = [
  {
    name:
      "native update preserves original ID, local storage, and granted permissions",
    async fn() {
      await fixture(async () => {
        const old = await manager().getAddonByID(ID);
        assert(old, "initial addon exists");
        const permissions = JSON.stringify(old.userPermissions);
        const { ExtensionPermissions } = ChromeUtils.importESModule(
          "resource://gre/modules/ExtensionPermissions.sys.mjs",
        );
        await ExtensionPermissions.add(ID, {
          permissions: ["alarms"],
          origins: [],
        });
        const optionalBefore = await ExtensionPermissions.get(ID);
        const stored = await command("set", "survives-update");
        assert(!stored.error, "storage set succeeds");
        const data = await packageVersion("2.0");
        const updater = new ChromeExtensionUpdaterClass((id) => {
          assertEquals(id, CRX_TEST_ID, "download uses original Chrome ID");
          return Promise.resolve(data);
        });
        const check = await updater.checkForUpdate(ID);
        assertEquals(check.status, "available", "new version available");
        assertEquals(check.kind, "update", "normal version update");
        assertEquals(check.currentVersion, "1.0", "source version");
        assertEquals(check.availableVersion, "2.0", "signed manifest version");
        const result = await updater.installUpdate(ID, browser());
        assertEquals(
          result.status,
          "installed",
          "normal native installation finishes",
        );
        assertEquals(result.version, "2.0", "installed version returned");
        await waitFor(
          () =>
            registry()
              .get(ID)
              ?.backgroundContext?.xulBrowser.contentTitle.includes(
                '"ready":true',
              ),
          "updated background ready",
        );
        const after = await manager().getAddonByID(ID);
        assert(after, "updated addon exists");
        assertEquals(after.id, ID, "internal identity retained");
        assertEquals(after.version, "2.0", "actual addon updated");
        assertEquals(
          JSON.stringify(after.userPermissions),
          permissions,
          "required permissions retained",
        );
        assertEquals(
          JSON.stringify(await ExtensionPermissions.get(ID)),
          JSON.stringify(optionalBefore),
          "granted optional permissions retained",
        );
        const read = await command("get");
        assertEquals(
          read.value,
          "survives-update",
          "native storage survives update",
        );
        assertEquals(read.id, CRX_TEST_ID, "extension sees stable Chrome ID");
        assertEquals(read.version, "2.0", "new execution context active");
        await rejected(
          updater.installUpdate(ID, browser()),
          "Check for an update",
        );
      });
    },
  },
  {
    name:
      "equal version requires explicit reconversion and older signed package never downgrades",
    async fn() {
      await fixture(async () => {
        let data = await packageVersion("1.0");
        const updater = new ChromeExtensionUpdaterClass(() =>
          Promise.resolve(data)
        );
        assertEquals(
          (await updater.checkForUpdate(ID)).status,
          "up-to-date",
          "equal versions do not update by default",
        );
        const reconvert = await updater.checkForUpdate(ID, { reconvert: true });
        assertEquals(
          reconvert.kind,
          "reconvert",
          "explicit same-version migration",
        );
        assertEquals(
          reconvert.status,
          "available",
          "same version is prepared only on request",
        );
        assertEquals(
          (await updater.installUpdate(ID, browser())).status,
          "installed",
          "normal AddonManager reconversion works",
        );
        data = await packageVersion("0.9");
        assertEquals(
          (await updater.checkForUpdate(ID, { reconvert: true })).status,
          "up-to-date",
          "reconversion cannot downgrade",
        );
        await rejected(
          updater.installUpdate(ID, browser()),
          "Check for an update",
        );
        assertEquals(
          (await manager().getAddonByID(ID))?.version,
          "1.0",
          "installed version remains unchanged",
        );
      });
    },
  },
  {
    name:
      "update checks reject tampering and signer-declared identity spoofing",
    async fn() {
      await fixture(async () => {
        const data = await packageVersion("2.0");
        new Uint8Array(data)[data.byteLength - 1] ^= 1;
        const updater = new ChromeExtensionUpdaterClass(() =>
          Promise.resolve(data)
        );
        await rejected(updater.checkForUpdate(ID), "signature");
        await rejected(
          updater.installUpdate(ID, browser()),
          "Check for an update",
        );
        assertEquals(
          (await manager().getAddonByID(ID))?.version,
          "1.0",
          "tampering cannot replace installed addon",
        );
        const impersonated = await packageVersion("2.0", { impersonate: true });
        const wrongSigner = new ChromeExtensionUpdaterClass(
          () => Promise.resolve(impersonated),
        );
        await rejected(wrongSigner.checkForUpdate(ID), "developer public key");
        await rejected(
          wrongSigner.installUpdate(ID, browser()),
          "Check for an update",
        );
        await rejected(
          updater.checkForUpdate("ordinary-firefox@example.test"),
          "Only installed converted",
        );
      });
    },
  },
  {
    name:
      "overlapping checks cannot replace the newest prepared update with an older response",
    async fn() {
      await fixture(async () => {
        const v2 = await packageVersion("2.0");
        const v3 = await packageVersion("3.0");
        let release: ((value: ArrayBuffer) => void) | undefined;
        let calls = 0;
        const updater = new ChromeExtensionUpdaterClass(async () => {
          calls++;
          return calls === 1
            ? await new Promise<ArrayBuffer>((resolve) => {
              release = resolve;
            })
            : v3;
        });
        const first = updater.checkForUpdate(ID);
        await waitFor(() => release, "first download waiting");
        const second = await updater.checkForUpdate(ID);
        assertEquals(
          second.availableVersion,
          "3.0",
          "newest check response retained",
        );
        release!(v2);
        await rejected(first, "superseded");
        assertEquals(
          (await updater.installUpdate(ID, browser())).version,
          "3.0",
          "newest prepared package installed",
        );
      });
    },
  },
  {
    name:
      "new permissions require the native update confirmation and rejection preserves old addon",
    async fn() {
      await fixture(async () => {
        const data = await packageVersion("2.0", {
          permissions: ["storage", "bookmarks"],
        });
        const updater = new ChromeExtensionUpdaterClass(() =>
          Promise.resolve(data)
        );
        await updater.checkForUpdate(ID);
        let prompted = false;
        const stop = observePrompts(
          "webextension-update-permission-prompt",
          (prompt) => {
            prompted = true;
            prompt.reject();
          },
        );
        try {
          await rejected(updater.installUpdate(ID, browser()), "cancelled");
        } finally {
          stop();
        }
        assert(prompted, "normal native permission difference prompt observed");
        assertEquals(
          (await manager().getAddonByID(ID))?.version,
          "1.0",
          "permission rejection retains old version",
        );
      });
    },
  },
  {
    name: "native downloaded-state cancellation settles the update request",
    async fn() {
      await fixture(async () => {
        const data = await packageVersion("2.0", {
          permissions: ["storage", "bookmarks"],
        });
        const updater = new ChromeExtensionUpdaterClass(() =>
          Promise.resolve(data)
        );
        await updater.checkForUpdate(ID);
        const stop = observePrompts(
          "webextension-update-permission-prompt",
          (prompt) => {
            prompt.install.cancel();
            prompt.reject();
          },
        );
        try {
          await rejected(updater.installUpdate(ID, browser()), "cancelled");
        } finally {
          stop();
        }
        assertEquals(
          (await manager().getAddonByID(ID))?.version,
          "1.0",
          "native cancellation retains installed package",
        );
      });
    },
  },
  {
    name:
      "closing the confirmation browser cancels the native update and removes its temporary package",
    async fn() {
      await fixture(async () => {
        const data = await packageVersion("2.0", {
          permissions: ["storage", "bookmarks"],
        });
        const updater = new ChromeExtensionUpdaterClass(() =>
          Promise.resolve(data)
        );
        await updater.checkForUpdate(ID);
        const popup = Services.ww.openWindow(
          Services.wm.getMostRecentWindow("navigator:browser")!,
          "chrome://browser/content/browser.xhtml",
          "_blank",
          "chrome,dialog=no,all",
          Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray),
        ) as unknown as Window & { gBrowser?: { selectedBrowser: Element } };
        try {
          const target = await waitFor(
            () =>
              popup.gBrowser?.selectedBrowser?.isConnected &&
              popup.gBrowser.selectedBrowser,
            "confirmation window browser",
          );
          const stop = observePrompts(
            "webextension-update-permission-prompt",
            (prompt) => {
              popup.addEventListener("unload", () => prompt.reject(), {
                once: true,
              });
              popup.close();
            },
          );
          try {
            await rejected(updater.installUpdate(ID, target), "closed");
          } finally {
            stop();
          }
          assertEquals(
            (await manager().getAddonByID(ID))?.version,
            "1.0",
            "window closure preserves installed version",
          );
        } finally {
          if (!popup.closed) popup.close();
        }
      });
    },
  },
  {
    name:
      "a competing native installation cancels an older update awaiting permission confirmation",
    async fn() {
      await fixture(async () => {
        const v2 = await packageVersion("2.0", {
          permissions: ["storage", "bookmarks"],
        });
        const v3 = await packageVersion("3.0");
        const updater = new ChromeExtensionUpdaterClass(() =>
          Promise.resolve(v2)
        );
        await updater.checkForUpdate(ID);
        let competing: Promise<void> | undefined;
        const stop = observePrompts(
          "webextension-update-permission-prompt",
          (prompt) => {
            competing = initialInstall(v3)
              .then(() => {})
              .finally(() => prompt.reject());
          },
        );
        try {
          await rejected(
            updater.installUpdate(ID, browser()),
            "Another installation changed",
          );
          assert(
            competing,
            "competing update began while old update awaited approval",
          );
          await competing;
        } finally {
          stop();
        }
        assertEquals(
          (await manager().getAddonByID(ID))?.version,
          "3.0",
          "waiting older package cannot downgrade externally installed version",
        );
      });
    },
  },
  {
    name:
      "runtime.onUpdateAvailable stages an update that can resume in the same session",
    async fn() {
      await fixture(
        async () => {
          const data = await packageVersion("2.0");
          const updater = new ChromeExtensionUpdaterClass(() =>
            Promise.resolve(data)
          );
          await updater.checkForUpdate(ID);
          const result = await updater.installUpdate(ID, browser());
          assertEquals(
            result.status,
            "postponed",
            "deferred native update reported",
          );
          assertEquals(result.version, "2.0", "staged version reported");
          assertEquals(
            (await manager().getAddonByID(ID))?.version,
            "1.0",
            "old context remains until extension reload",
          );
          const pending = (await manager().getAllInstalls()).find(
            (install) =>
              install.addon?.id === ID &&
              install.state === manager().STATE_POSTPONED,
          );
          assert(pending, "native update remains staged after service returns");
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
                finish(
                  new Error(`Resuming staged update failed (${pending.error})`),
                );
              },
            };
            const timer = setTimeout(
              () => finish(new Error("Resuming staged update timed out")),
              15000,
            );
            pending.addListener(listener);
            pending.continuePostponedInstall();
          });
          assertEquals(
            (await manager().getAddonByID(ID))?.version,
            "2.0",
            "staged native update can resume after service returns",
          );
        },
        { postpone: true },
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("ChromeExtensionUpdater.test.mts", tests);
}
