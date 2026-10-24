// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zipSync } from "fflate/browser";
import type {
  NativeExtension,
  NativeManager,
  PermissionPrompt,
  Reply,
  UpdateWorkerSnapshot,
} from "./helpers/update-test-types.ts";
import type {
  NativeCorpusTab,
  NativeCorpusWindow,
} from "../../../../../tools/chrome-extension-runtime/native-test-types.ts";
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
  assertEquals(
    context.viewType,
    "background_worker",
    "compiled Worker runs the fixture",
  );
  assert(!context.xulBrowser, "the Worker has no background document browser");
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as NativeCorpusWindow;
  let tab: NativeCorpusTab | undefined;
  const sequence = ++request;
  try {
    // Use an ordinary extension page as the sender. A compiled Worker has
    // neither a document title nor a message manager to drive directly.
    tab = win.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    const control = tab.linkedBrowser;
    await waitFor(
      () => control.contentTitle === "updater-control-ready",
      "extension sender page",
    );
    const payload = {
      updaterFixture: true,
      request: sequence,
      operation,
      value,
    };
    control.messageManager.loadFrameScript(
      `data:application/javascript,${
        encodeURIComponent(
          `content.postMessage(${JSON.stringify(payload)}, '*');`,
        )
      }`,
      false,
    );
    const reply = await waitFor(() => {
      try {
        const reply = JSON.parse(control.contentTitle) as Reply;
        return reply.request === sequence ? reply : false;
      } catch {
        return false;
      }
    }, "actual Worker storage response");
    assert(!reply.error, `Worker command succeeds: ${reply.error}`);
    assert(
      reply.noDocument && reply.noWindow,
      "the response comes from a documentless Worker",
    );
    assert(
      typeof reply.contextId === "string" && reply.contextId.length > 0,
      "the Worker reports its public context UUID",
    );
    return reply;
  } finally {
    if (tab) win.gBrowser.removeTab(tab);
  }
}

export async function workerSnapshot(): Promise<UpdateWorkerSnapshot> {
  const reply = await command("get");
  const extension = registry().get(ID);
  const context = extension?.backgroundContext;
  assert(
    extension && context,
    "the responding native Worker remains registered",
  );
  assert(
    typeof reply.contextId === "string",
    "public Worker context UUID exists",
  );
  return { extension, context, contextId: reply.contextId };
}

export async function assertWorkerReplaced(
  before: UpdateWorkerSnapshot,
  version: string,
): Promise<void> {
  await waitFor(
    () => before.context.unloaded,
    "old native Worker context termination",
  );
  assert(
    !before.extension.views.has(before.context),
    "old context leaves the actual extension view registry",
  );
  const current = await workerSnapshot();
  assert(
    current.extension !== before.extension,
    "native update creates a fresh extension instance",
  );
  assert(
    current.context !== before.context,
    "native update creates a fresh Worker context",
  );
  assert(
    current.contextId !== before.contextId,
    "the new Worker has a different public context UUID",
  );
  const reply = await command("get");
  assertEquals(
    reply.version,
    version,
    "the fresh Worker executes the updated package",
  );
  assertEquals(
    reply.id,
    CRX_TEST_ID,
    "the fresh Worker retains the original Chrome ID",
  );
  console.log(
    "UPDATER_WORKER_EVIDENCE",
    JSON.stringify({
      version,
      originalId: reply.id,
      oldContextId: before.contextId,
      newContextId: current.contextId,
      oldUnloaded: before.context.unloaded,
      viewType: current.context.viewType,
      noDocument: reply.noDocument,
      noWindow: reply.noWindow,
    }),
  );
}

export async function packageVersion(
  version: string,
  options: {
    permissions?: string[];
    postpone?: boolean;
    impersonate?: boolean;
    scriptType?: "classic" | "module";
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
          background: {
            service_worker: "background.js",
            ...(options.scriptType === "module" ? { type: "module" } : {}),
          },
        }),
      ),
      "background.js": encoder.encode(`
      ${
        options.scriptType === "module"
          ? "export const moduleFixtureURL = import.meta.url;"
          : ""
      }
      let pending;
      ${
        options.postpone
          ? "browser.runtime.onUpdateAvailable.addListener(update => { pending = update.version; });"
          : ""
      }
      browser.runtime.onMessage.addListener(data => {
        if (!data || data.updaterFixture !== true) return undefined;
        return (async () => {
          try {
            if (data.operation === 'set') await browser.storage.local.set({ retained: data.value });
            const stored = await browser.storage.local.get('retained');
            const contexts = await browser.runtime.getContexts({ contextTypes: ['BACKGROUND'] });
            if (contexts.length !== 1) throw new Error('Expected exactly one actual background Worker');
            return { request: data.request, value: stored.retained,
              id: browser.runtime.id, version: browser.runtime.getManifest().version, pending,
              contextId: contexts[0].contextId,
              noDocument: typeof document === 'undefined', noWindow: typeof window === 'undefined' };
          } catch (error) { return { request: data.request, error: String(error) }; }
        })();
      });
    `),
      "control.html": encoder.encode(
        '<!doctype html><script src="control.js"></script>',
      ),
      "control.js": encoder.encode(`
        document.title = 'updater-control-ready';
        window.addEventListener('message', async event => {
          if (!event.data || event.data.updaterFixture !== true) return;
          try { document.title = JSON.stringify(await browser.runtime.sendMessage(event.data)); }
          catch (error) { document.title = JSON.stringify({ request: event.data.request, error: String(error) }); }
        });
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
    const ready = await command("get");
    assertEquals(
      ready.id,
      CRX_TEST_ID,
      "initial Worker sees the original signed Chrome ID",
    );
    const installed = await manager().getAddonByID(ID);
    assert(
      installed && !installed.temporarilyInstalled,
      "fixture uses permanent AddonManager install",
    );
    assertEquals(
      ready.version,
      installed.version,
      "the Worker executes the installed package version",
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
  options: { postpone?: boolean; scriptType?: "classic" | "module" } = {},
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
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  try {
    assert(
      AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
      "compiled Worker bindings are required for update regressions",
    );
    assertEquals(
      WebExtensionPolicy.backgroundServiceWorkerEnabled,
      false,
      "the global experimental Worker policy remains disabled",
    );
    const converted = await initialInstall(
      await packageVersion("1.0", options),
    );
    await fn(converted);
  } finally {
    try {
      for (const install of await manager().getAllInstalls()) {
        if (
          install.addon?.id === ID &&
          install.state === manager().STATE_POSTPONED
        ) {
          install.cancel();
        }
      }
      const finalContext = registry().get(ID)?.backgroundContext;
      const addon = await manager().getAddonByID(ID);
      if (addon) await addon.uninstall();
      if (finalContext) {
        await waitFor(
          () => finalContext.unloaded,
          "final native Worker context termination",
        );
      }
      await waitFor(
        () => !registry().has(ID),
        "extension removal from the native registry",
      );
    } finally {
      if (hadPref) {
        Services.prefs.setBoolPref("xpinstall.signatures.required", signing);
      } else Services.prefs.clearUserPref("xpinstall.signatures.required");
    }
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
        const beforeWorker = await workerSnapshot();
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
        await assertWorkerReplaced(beforeWorker, "2.0");
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
      "signed module Worker update preserves storage and replaces the actual execution context",
    async fn() {
      await fixture(async () => {
        await command("set", "module-update-storage");
        const beforeWorker = await workerSnapshot();
        const data = await packageVersion("2.0", { scriptType: "module" });
        const updater = new ChromeExtensionUpdaterClass(() =>
          Promise.resolve(data)
        );
        await updater.checkForUpdate(ID);
        assertEquals(
          (await updater.installUpdate(ID, browser())).status,
          "installed",
          "native module update finishes",
        );
        await assertWorkerReplaced(beforeWorker, "2.0");
        assertEquals(
          (await command("get")).value,
          "module-update-storage",
          "module Worker storage survives replacement",
        );
      }, { scriptType: "module" });
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
        const beforeWorker = await workerSnapshot();
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
        await assertWorkerReplaced(beforeWorker, "1.0");
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
        const beforeWorker = await workerSnapshot();
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
        await assertWorkerReplaced(beforeWorker, "3.0");
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
        const beforeWorker = await workerSnapshot();
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
        await assertWorkerReplaced(beforeWorker, "3.0");
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
          const beforeWorker = await workerSnapshot();
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
          const postponedReply = await command("get");
          assertEquals(
            postponedReply.contextId,
            beforeWorker.contextId,
            "postponement retains the running Worker context",
          );
          assertEquals(
            postponedReply.pending,
            "2.0",
            "the real Worker received runtime.onUpdateAvailable",
          );
          assert(
            !beforeWorker.context.unloaded,
            "postponement does not terminate the old Worker",
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
          await assertWorkerReplaced(beforeWorker, "2.0");
        },
        { postpone: true },
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("ChromeExtensionUpdater.test.mts", tests);
}
