// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type { NativeCorpusExtensionParent } from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
import type { PrivilegedCacheScope } from "../helpers/worker-cache-test-types.ts";

const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
) as { ExtensionParent: NativeCorpusExtensionParent };

async function rejectedPut(
  cache: Cache,
  url: string,
  scope: PrivilegedCacheScope,
): Promise<void> {
  try {
    await cache.put(url, new scope.Response("must not be stored"));
  } catch (error: unknown) {
    assert(
      typeof error === "object" && error !== null && "name" in error,
      "Native Cache rejection supplies an error",
    );
    assertEquals(
      error.name,
      "TypeError",
      "Invalid cache-key scheme is rejected",
    );
    assertEquals(await cache.match(url), undefined, "Rejected key is absent");
    return;
  }
  throw Error("Native Cache unexpectedly stored a disallowed extension URL");
}

async function testNativeScriptCacheGate(): Promise<void> {
  console.log("WORKER_NATIVE_CACHE_STAGE", "start");
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Native bindings are compiled",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "The global experimental worker preference remains disabled",
  );
  const original = "aacceeggiikkmmooaacceeggiikkmmoo";
  const ids = [original + "@chromium-extension", "native-cache-ordinary@tests"];
  const installed: { uninstall(): Promise<void> }[] = [];
  const paths: string[] = [];
  const cleanup: (() => Promise<unknown>)[] = [];
  let sandbox: object | undefined;
  const scriptBody = "globalThis.nativeScriptCacheProbe = 'persisted';";
  try {
    for (const [index, id] of ids.entries()) {
      const manifest = {
        manifest_version: 3,
        name: "Native script cache " + index,
        version: "1.0",
        browser_specific_settings: {
          gecko: {
            id,
            ...(index === 0
              ? {
                floorp_chrome_extension: {
                  schema_version: 1,
                  original_id: original,
                },
              }
              : {}),
          },
        },
      };
      const path = PathUtils.join(
        PathUtils.tempDir,
        "native-script-cache-" + Services.uuid.generateUUID() + ".xpi",
      );
      paths.push(path);
      await IOUtils.write(
        path,
        zipSync({
          "manifest.json": new TextEncoder().encode(JSON.stringify(manifest)),
          "worker-cache-probe.js": new TextEncoder().encode(scriptBody),
        }),
      );
      installed.push(
        await AddonManager.installTemporaryAddon(new FileUtils.File(path)),
      );
    }
    const compatible = ExtensionParent.GlobalManager.extensionMap.get(ids[0]!);
    const ordinary = ExtensionParent.GlobalManager.extensionMap.get(ids[1]!);
    assert(
      compatible && ordinary,
      "Both genuine extension policies are active",
    );
    sandbox = Cu.Sandbox(compatible.principal, {
      sandboxName: "Native internal script cache regression",
      wantGlobalProperties: ["caches", "fetch"],
    });
    const scope = sandbox as PrivilegedCacheScope;
    const compatibleURL = compatible.baseURI.spec + "worker-cache-probe.js";
    const ordinaryURL = ordinary.baseURI.spec + "ordinary-cache-probe.js";
    const unknownURL = "moz-extension://" + Services.uuid.generateUUID()
      .toString().replace(/[{}]/g, "") +
      "/unknown-cache-probe.js";
    const name = "native-worker-cache-" + Services.uuid.generateUUID();
    console.log("WORKER_NATIVE_CACHE_STAGE", "construct native storage");
    const internal = new scope.CacheStorage("chrome", compatible.principal);
    const content = new scope.CacheStorage("content", compatible.principal);
    cleanup.push(() => internal.delete(name), () => content.delete(name));
    console.log("WORKER_NATIVE_CACHE_STAGE", "open internal cache");
    const cache = await internal.open(name);
    console.log("WORKER_NATIVE_CACHE_STAGE", "put compatible script key");
    const fetched = await scope.fetch(compatibleURL);
    assert(fetched.ok, "The real extension resource is fetched successfully");
    await cache.put(compatibleURL, fetched);
    const response = await cache.match(compatibleURL);
    assert(response, "Native internal cache stores the compatible script key");
    assertEquals(
      await response.text(),
      scriptBody,
      "The native cache returns its genuine persisted response body",
    );
    await rejectedPut(cache, ordinaryURL, scope);
    await rejectedPut(cache, unknownURL, scope);
    await rejectedPut(await content.open(name), compatibleURL, scope);
    console.log(
      "WORKER_NATIVE_CACHE_EVIDENCE",
      JSON.stringify({
        internalReadback: true,
        nonOptInRejected: true,
        unknownRejected: true,
        publicCacheUnchanged: true,
        globalPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
      }),
    );
  } finally {
    for (const clean of cleanup.reverse()) await clean();
    if (sandbox) Cu.nukeSandbox(sandbox);
    for (const addon of installed.reverse()) await addon.uninstall();
    for (const path of paths) {
      await IOUtils.remove(path, { ignoreAbsent: true });
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WorkerScriptCacheNative.test.mts", [{
    name: "native internal script-cache policy with global workers disabled",
    fn: testNativeScriptCacheGate,
  }]);
}
