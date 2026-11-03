// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type {
  CommandResult,
  HTTPObservation,
  ScopeExtensionParent,
  ScopeObservation,
  ScopeWindow,
} from "../helpers/worker-background-scope-test-types.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const workerBody = `
const ready=(async()=>{const {starts=0}=await browser.storage.local.get("starts");await browser.storage.local.set({starts:starts+1});return starts+1;})();
browser.runtime.onMessage.addListener(message=>{
  if(message.scopeOracle!==true)return;
  return ready.then(async starts=>({starts,worker:self instanceof ServiceWorkerGlobalScope,hasDocument:typeof document!=="undefined",contexts:await browser.runtime.getContexts({})}));
});
`;
const controlBody = `
document.title="scope-control-ready";
window.addEventListener("message",async event=>{
  if(event.data.scopeOracle!==true)return;
  try {const result=await browser.runtime.sendMessage(event.data);document.title=JSON.stringify({requestId:event.data.requestId,result});}
  catch(error){document.title=JSON.stringify({requestId:event.data.requestId,error:String(error)});}
});
`;

async function waitFor<T>(
  read: () => T | undefined | false,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out: ${description}`);
}
async function bounded<T>(promise: Promise<T>, label: string): Promise<T> {
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    return await Promise.race([
      promise,
      new Promise<never>((_, reject) => {
        timer = setTimeout(
          () => reject(new Error(`Timed out: ${label}`)),
          10000,
        );
      }),
    ]);
  } finally {
    if (timer !== undefined) clearTimeout(timer);
  }
}
function errorRecord(
  error: unknown,
): { name: string; message: string; result: number | null } {
  const value = error as {
    name?: unknown;
    message?: unknown;
    result?: unknown;
  };
  return {
    name: typeof value?.name === "string" ? value.name : "Unknown",
    message: typeof value?.message === "string" ? value.message : String(error),
    result: typeof value?.result === "number" ? value.result : null,
  };
}

async function extensionCase(
  index: number,
  nested: boolean,
  module: boolean,
  trustedChrome = true,
): Promise<ScopeObservation> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as { ExtensionParent: ScopeExtensionParent };
  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager,
  );
  const originalId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" + "bcdef"[index];
  const id = trustedChrome
    ? originalId + "@chromium-extension"
    : "scope-control@ordinary-firefox.invalid";
  const script = nested ? "nested/worker.js" : "worker.js";
  const observation: ScopeObservation = {
    label: `${trustedChrome ? "chrome" : "firefox"}-${
      nested ? "nested" : "root"
    }-${module ? "module" : "classic"}`,
    declaredScript: script,
    declaredType: module ? "module" : "classic",
    trustedChrome,
  };
  const manifest = {
    manifest_version: 3,
    name: observation.label,
    version: "1.0",
    permissions: ["storage"],
    background: {
      service_worker: script,
      ...(module ? { type: "module" } : {}),
    },
    browser_specific_settings: {
      gecko: {
        id,
        ...(trustedChrome
          ? {
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: originalId,
            },
          }
          : {}),
      },
    },
  };
  const files = {
    "manifest.json": JSON.stringify(manifest),
    [script]: workerBody,
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js": controlBody,
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `scope-oracle-${Services.uuid.generateUUID()}.xpi`,
  );
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as ScopeWindow;
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  let addon: { uninstall(): Promise<void> } | undefined;
  let control: ReturnType<ScopeWindow["gBrowser"]["addTab"]> | undefined;
  await IOUtils.write(
    path,
    zipSync(
      Object.fromEntries(
        Object.entries(files).map((
          [name, text],
        ) => [name, new TextEncoder().encode(text)]),
      ),
    ),
  );
  try {
    addon = await bounded(
      AddonManager.installTemporaryAddon(new FileUtils.File(path)),
      "temporary authored install",
    );
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "authored extension map",
    );
    observation.policyAllowed = extension.policy.backgroundServiceWorkerAllowed;
    observation.policyModule = extension.policy.backgroundTypeModule;
    observation.extensionRoot = extension.baseURI.spec;
    // This exact existing registration API is also used by actual extension startup.
    // Retain its real rejection before any generic readiness timeout.
    await bounded(
      swm.registerForAddonPrincipal(extension.principal),
      "actual registerForAddonPrincipal",
    );
    const registration = swm.getRegistrationForAddonPrincipal(
      extension.principal,
    );
    observation.registration = {
      scope: registration.scope,
      scriptSpec: registration.scriptSpec,
      workerId: registration.activeWorker?.id ?? null,
    };
    await waitFor(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual Worker context",
    );
    observation.backgroundViewType = extension.backgroundContext?.viewType ??
      null;
    control = win.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: principal,
    });
    await waitFor(
      () => control?.linkedBrowser.contentTitle === "scope-control-ready",
      "ordinary extension messaging page",
    );
    let requestId = 0;
    const command = async () => {
      const current = ++requestId;
      control?.linkedBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            `content.postMessage(${
              JSON.stringify({ scopeOracle: true, requestId: current })
            },"*");`,
          ),
        false,
      );
      return await waitFor(() => {
        const title = control?.linkedBrowser.contentTitle;
        if (!title?.startsWith("{")) return false;
        const response = JSON.parse(title) as CommandResult;
        if (response.requestId !== current) return false;
        if (response.error) throw new Error(response.error);
        return response.result;
      }, "real runtime message response");
    };
    observation.before = await command();
    const registrationWorkerId =
      swm.getRegistrationForAddonPrincipal(extension.principal).activeWorker.id;
    await bounded(extension.terminateBackground(), "actual Worker stop");
    await waitFor(
      () =>
        extension.backgroundState === "stopped" && !extension.backgroundContext,
      "actual stopped/unloaded Worker",
    );
    observation.stopped = true;
    observation.after = await command();
    observation.sameRegistrationAfterRestart =
      swm.getRegistrationForAddonPrincipal(extension.principal).activeWorker
        .id === registrationWorkerId;
  } catch (error) {
    observation.error = errorRecord(error);
  } finally {
    if (control) win.gBrowser.removeTab(control, { skipPermitUnload: true });
    if (addon) await addon.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
    observation.cleanup = !ExtensionParent.GlobalManager.extensionMap.has(id) &&
      !await IOUtils.exists(path);
  }
  console.log(
    "[WorkerBackgroundScopeNative] case",
    JSON.stringify(observation),
  );
  return observation;
}

async function oracle(): Promise<void> {
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "ordinary Firefox global Worker preference stays disabled",
  );
  const observations: ScopeObservation[] = [];
  for (let index = 0; index < 4; index++) {
    observations.push(await extensionCase(index, index % 2 === 1, index >= 2));
  }
  observations.push(await extensionCase(4, true, false, false));
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as ScopeWindow;
  const httpURL = new URL(
    "worker-background-scope.html",
    win.wrappedJSObject.__floorpRuntimeFixtureURL,
  ).href;
  assert(httpURL.startsWith("http://127.0.0.1:"), "owned loopback control URL");
  const tab = win.gBrowser.addTab(httpURL, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  let http: HTTPObservation | undefined;
  let httpError: ReturnType<typeof errorRecord> | undefined;
  try {
    http = await waitFor(() => {
      const title = tab.linkedBrowser.contentTitle;
      return title.startsWith("scope-http:")
        ? JSON.parse(title.slice(11)) as HTTPObservation
        : false;
    }, "actual HTTP page scope controls");
  } catch (error) {
    httpError = errorRecord(error);
  } finally {
    win.gBrowser.removeTab(tab, { skipPermitUnload: true });
  }
  const report = {
    componentDiagnostic: true,
    corpusPrimary: false,
    globalWorkerPolicyBefore: false,
    globalWorkerPolicyAfter: WebExtensionPolicy.backgroundServiceWorkerEnabled,
    workerBody,
    observations,
    http,
    httpError,
  };
  // Retain every real outcome before the strict regression can fail on old builds.
  console.log(
    "[WorkerBackgroundScopeNative] all-observations",
    JSON.stringify(report),
  );
  for (const actual of observations.filter((value) => value.trustedChrome)) {
    assert(
      !actual.error,
      `${actual.label}: actual main script registration succeeds (${actual.error?.name}: ${actual.error?.message})`,
    );
    assert(actual.cleanup, `${actual.label}: owned extension and XPI removed`);
    assert(
      actual.registration &&
        actual.registration.scope === actual.extensionRoot &&
        actual.registration.scriptSpec.endsWith(actual.declaredScript),
      `${actual.label}: exact root registration and original declared path`,
    );
    assertEquals(
      actual.policyModule,
      actual.declaredType === "module",
      `${actual.label}: original module type`,
    );
    assertEquals(
      actual.backgroundViewType,
      "background_worker",
      `${actual.label}: actual Worker context`,
    );
    assert(
      actual.before?.worker && !actual.before.hasDocument &&
        actual.before.starts === 1,
      `${actual.label}: real storage/runtime API in Worker`,
    );
    assert(
      actual.stopped && actual.after?.starts === 2 &&
        actual.sameRegistrationAfterRestart,
      `${actual.label}: real message restarts stopped Worker and preserves registration/storage`,
    );
    const oldContext = actual.before.contexts.find((value) =>
      value.contextType === "BACKGROUND"
    );
    const newContext = actual.after.contexts.find((value) =>
      value.contextType === "BACKGROUND"
    );
    assert(
      oldContext && newContext && oldContext.contextId !== newContext.contextId,
      `${actual.label}: restarted context has a new actual identity`,
    );
  }
  const firefox = observations[4];
  assert(
    firefox.error && !firefox.registration && firefox.cleanup,
    "ordinary Firefox Worker stays rejected without global preference",
  );
  assert(http && !httpError, "ordinary HTTP controls completed");
  const origin = new URL(httpURL).origin;
  assertEquals(
    http.root.scope,
    origin + "/",
    "ordinary HTTP root script/root scope succeeds",
  );
  assertEquals(
    http.nestedRoot.error,
    "SecurityError",
    "ordinary nested HTTP script cannot claim root scope without header",
  );
  assertEquals(
    http.nestedDefault.scope,
    origin + "/worker-background-scope/",
    "ordinary nested HTTP default-directory scope succeeds",
  );
  assert(
    http.unregistered.length === 2 && http.unregistered.every(Boolean),
    "both actual HTTP registrations removed",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "no global Worker preference changed",
  );
}
export async function runAllTests(): Promise<void> {
  await runTests("WorkerBackgroundScopeNative.test.mts", [{
    name:
      "validated Chrome declared background root scope, classic/module stop-restart, ordinary scope boundaries",
    fn: oracle,
  }]);
}
