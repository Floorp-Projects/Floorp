// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { managedStorageProbeSource } from "./helpers/managed-storage-probe.mjs";
import type {
  ManagedStorageAddon,
  ManagedStorageParentImport,
  ManagedStorageReply,
  ManagedStorageResult,
  ManagedStorageWindow,
} from "./helpers/managed-storage-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function waitFor<T>(
  read: () => T | false | undefined,
  label: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const result = read();
    if (result) return result;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${label}`);
}

function checkResult(
  result: ManagedStorageResult,
  id: string,
  converted: boolean,
  worker: boolean,
  module: boolean,
): void {
  assertEquals(result.publicId, id, "Actual public extension identity");
  assertEquals(result.worker, worker, "Actual ServiceWorkerGlobalScope");
  assertEquals(result.hasDocument, !worker, "Actual document availability");
  assertEquals(result.module, module, "Module-only worker syntax executed");
  assertEquals(result.observations.map((row) => row.name), [
    "all",
    "string",
    "array",
    "defaults",
    "keys",
    "bytes",
    "callback",
    "set",
    "remove",
    "clear",
    "afterWrites",
  ], "All real public operations completed");
  for (const row of result.observations) {
    if (["set", "remove", "clear"].includes(row.name)) {
      assert(
        !!row.error && /read.only/i.test(row.error),
        `${row.name} preserves managed read-only rejection`,
      );
    } else if (row.name === "bytes") {
      assert(!row.error, "Byte count did not fail");
      assertEquals(row.value, 0, "Managed bytes remain zero");
    } else if (converted) {
      assert(
        !row.error,
        `${row.name}: ${row.error || "read succeeds without policy"}`,
      );
      assertEquals(
        row.value,
        row.name === "keys"
          ? []
          : row.name === "defaults"
          ? { missing: "fallback", count: 7, nested: { fixed: true } }
          : {},
        `${row.name} matches the Chrome no-policy store`,
      );
    } else {
      assert(
        !!row.error && row.error.includes("Managed storage manifest not found"),
        `${row.name} preserves Firefox missing-manifest error`,
      );
    }
  }
}

async function managedCase(
  index: number,
  mode: "classic" | "module" | "ordinary" | "mismatch",
): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as ManagedStorageParentImport;
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as ManagedStorageWindow;
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const originalId = "a".repeat(31) + "bcde"[index];
  const converted = mode === "classic" || mode === "module";
  const id = converted
    ? `${originalId}@chromium-extension`
    : `managed-${mode}@ordinary-firefox.invalid`;
  const tabs: ReturnType<ManagedStorageWindow["gBrowser"]["addTab"]>[] = [];
  const selected = win.gBrowser.selectedTab;
  const fixture = win.wrappedJSObject.__floorpRuntimeFixtureURL;
  const fixtureURL = new URL(fixture);
  const manifest = {
    manifest_version: converted ? 3 : 2,
    name: `Managed storage actual ${mode}`,
    version: "1.0",
    permissions: ["storage"],
    background: converted
      ? {
        service_worker: "worker.js",
        ...(mode === "module" ? { type: "module" } : {}),
      }
      : { scripts: ["background.js"] },
    ...(mode === "classic"
      ? { storage: { managed_schema: "managed-schema.json" } }
      : {}),
    ...(converted
      ? {
        content_scripts: [{
          matches: [fixtureURL.protocol + "//" + fixtureURL.hostname + "/*"],
          js: ["content.js"],
          run_at: "document_idle",
        }],
      }
      : {}),
    browser_specific_settings: {
      gecko: {
        id,
        ...((converted || mode === "mismatch")
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
  const worker = `${
    mode === "module"
      ? "export const managedModuleWitness = true; globalThis.managedModule = managedModuleWitness;\n"
      : ""
  }${managedStorageProbeSource}\nchrome.runtime.onMessage.addListener((message, sender, sendResponse) => {if(message.managedStorageFixture !== true)return; runManagedStorageProbe().then(sendResponse); return true;});`;
  const page =
    `${managedStorageProbeSource}\nwindow.addEventListener('message', async event => {const data = event.data; if(data?.managedStorageFixture !== true)return; try {if(data.operation === 'restrict'){await chrome.storage.managed.setAccessLevel({accessLevel:'TRUSTED_CONTEXTS'}); document.title=JSON.stringify({requestId:data.requestId, restricted:true});}else {const result = data.operation === 'worker' ? await chrome.runtime.sendMessage(data) : await runManagedStorageProbe(); document.title=JSON.stringify({requestId:data.requestId,result});}}catch(error){document.title=JSON.stringify({requestId:data.requestId,error:String(error)});}}); document.title='managed-control-ready';`;
  const content =
    `chrome.storage.managed.get(null).then(()=>{document.title=JSON.stringify({requestId:1,denied:false});},error=>{document.title=JSON.stringify({requestId:1,denied:/Access to storage is not allowed/.test(String(error?.message||error)),error:String(error?.message||error)});});`;
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "worker.js": worker,
    "background.js": "globalThis.managedBackgroundReady=true;",
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js": page,
    "content.js": content,
    "managed-schema.json": JSON.stringify({
      type: "object",
      properties: { managedValue: { type: "string" } },
    }),
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `managed-storage-${Services.uuid.generateUUID()}.xpi`,
  );
  let addon: ManagedStorageAddon | undefined;
  const failures: unknown[] = [];
  try {
    await IOUtils.write(
      path,
      zipSync(
        Object.fromEntries(
          Object.entries(files).map((
            [name, body],
          ) => [name, new TextEncoder().encode(body)]),
        ),
      ),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "actual authored extension",
    );
    if (converted) {
      await waitFor(
        () => extension.backgroundContext?.viewType === "background_worker",
        "native managed Worker",
      );
      assertEquals(
        extension.policy.backgroundServiceWorkerAllowed,
        true,
        "Only converted extension receives Worker policy",
      );
      assertEquals(
        extension.policy.backgroundTypeModule,
        mode === "module",
        "Actual registered Worker type",
      );
    } else {
      assertEquals(
        extension.policy.backgroundServiceWorkerAllowed,
        false,
        "Ordinary and mismatched identities retain Firefox Worker policy",
      );
      await waitFor(
        () => extension.backgroundContext?.viewType === "background",
        "ordinary Firefox background document",
      );
    }
    const control = win.gBrowser.addTab(
      extension.baseURI.resolve("control.html"),
      { triggeringPrincipal: principal },
    );
    tabs.push(control);
    win.gBrowser.selectedTab = control;
    await waitFor(
      () => control.linkedBrowser.contentTitle === "managed-control-ready",
      "real control document",
    );
    let sequence = 0;
    const command = async (operation: string): Promise<ManagedStorageReply> => {
      const requestId = ++sequence;
      control.linkedBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            `content.postMessage(${
              JSON.stringify({
                managedStorageFixture: true,
                requestId,
                operation,
              })
            }, '*');`,
          ),
        false,
      );
      return await waitFor(() => {
        try {
          const reply = JSON.parse(
            control.linkedBrowser.contentTitle,
          ) as ManagedStorageReply;
          return reply.requestId === requestId ? reply : false;
        } catch {
          return false;
        }
      }, `public managed ${operation}`);
    };
    const doc = await command("document");
    assert(
      !doc.error && !!doc.result,
      `Document completes: ${doc.error || "result"}`,
    );
    checkResult(
      doc.result!,
      converted ? originalId : id,
      converted,
      false,
      false,
    );
    if (converted) {
      const reply = await command("worker");
      assert(
        !reply.error && !!reply.result,
        `Worker completes: ${reply.error || "result"}`,
      );
      checkResult(reply.result!, originalId, true, true, mode === "module");
      assert(
        (await command("restrict")).restricted === true,
        "Real public access-level mutation completed",
      );
      const url = new URL("webpage-messaging.html", fixture);
      url.searchParams.set("managed", mode);
      const webpage = win.gBrowser.addTab(url.href, {
        triggeringPrincipal: principal,
      });
      tabs.push(webpage);
      const denied = await waitFor(() => {
        try {
          const reply = JSON.parse(
            webpage.linkedBrowser.contentTitle,
          ) as ManagedStorageReply;
          return reply.requestId === 1 ? reply : false;
        } catch {
          return false;
        }
      }, "owned content-script storage denial");
      assertEquals(
        denied.denied,
        true,
        "Empty-store fallback cannot bypass content access level",
      );
    }
  } catch (error) {
    failures.push(error);
  } finally {
    for (const tab of tabs) {
      try {
        win.gBrowser.removeTab(tab, { skipPermitUnload: true });
      } catch (error) {
        failures.push(error);
      }
    }
    try {
      if (win.gBrowser.tabs.includes(selected)) {
        win.gBrowser.selectedTab = selected;
      }
    } catch (error) {
      failures.push(error);
    }
    try {
      if (addon) await addon.uninstall();
    } catch (error) {
      failures.push(error);
    }
    try {
      await IOUtils.remove(path, { ignoreAbsent: true });
    } catch (error) {
      failures.push(error);
    }
  }
  assert(
    !ExtensionParent.GlobalManager.extensionMap.has(id),
    "Authored extension uninstalled",
  );
  assert(!await IOUtils.exists(path), "Authored XPI removed");
  if (failures.length) {
    throw new AggregateError(failures, `Managed fixture ${mode} failed`);
  }
}

export async function runAllTests(): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Actual compiled native Worker runtime required",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "Ordinary Worker policy remains disabled",
  );
  await runTests("ChromeManagedStorageNative", [
    {
      name:
        "Chrome managed store DOC and classic Worker with no configured policy",
      fn: () => managedCase(0, "classic"),
    },
    {
      name: "Chrome managed store DOC and module Worker without managed schema",
      fn: () => managedCase(1, "module"),
    },
    {
      name: "Ordinary Firefox missing managed manifest remains an error",
      fn: () => managedCase(2, "ordinary"),
    },
    {
      name: "Mismatched Chrome marker cannot obtain an empty managed store",
      fn: () => managedCase(3, "mismatch"),
    },
  ]);
}
