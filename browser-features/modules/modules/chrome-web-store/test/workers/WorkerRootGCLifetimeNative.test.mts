// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type {
  NativeCorpusAddon,
  NativeCorpusExtensionParent,
  NativeCorpusTab,
  NativeCorpusWindow,
} from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
import type {
  WorkerRootGCResponses,
  WorkerRootGCSnapshot,
} from "../helpers/worker-root-gc-test-types.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
) as { ExtensionParent: NativeCorpusExtensionParent };
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

const workerProgram = `
browser.authoredRootValue='retained-through-real-GC';
const rootWeak=new WeakRef(browser);
const runtimeWeak=new WeakRef(browser.runtime);
const methodWeak=new WeakRef(browser.runtime.getURL);
let sentinelWeak;
browser.runtime.onMessage.addListener(message=>{
  if(message.command==='arm'){
    sentinelWeak=new WeakRef({collectable:true});
    return Promise.resolve({token:message.token,armed:true});
  }
  if(message.command!=='probe')return;
  return(async()=>{
    const root=rootWeak.deref(),runtime=runtimeWeak.deref(),method=methodWeak.deref();
    const result={token:message.token,sentinelCollected:sentinelWeak.deref()===undefined,rootAlive:!!root,runtimeAlive:!!runtime,methodAlive:!!method};
    if(!root||!runtime||!method)return result;
    await browser.storage.local.set({rootGCProof:{token:message.token}});
    const {rootGCProof}=await browser.storage.local.get('rootGCProof');
    return{...result,sameRoot:browser===root,sameRuntime:browser.runtime===runtime,sameMethod:browser.runtime.getURL===method,authoredValue:browser.authoredRootValue,url:method.call({},'gc-proof'),storage:rootGCProof};
  })();
});`;
const controlProgram = `
document.title='gc-control-ready';
window.addEventListener('message',async event=>{
  if(event.data.gcAck){document.title=JSON.stringify({gcAck:event.data.gcAck});return;}
  if(!event.data.rootGCCommand)return;
  try{document.title=JSON.stringify(await browser.runtime.sendMessage(event.data));}
  catch(error){document.title=JSON.stringify({token:event.data.token,error:String(error)});}
});`;

async function until<T>(
  read: () => T | undefined | false,
  label: string,
): Promise<T> {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw Error("Timed out waiting for " + label);
}

async function testRootLifetime(type: "classic" | "module"): Promise<void> {
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Native Worker bindings are compiled",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "The global experimental Worker policy remains disabled",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as NativeCorpusWindow;
  const original = type === "classic"
    ? "accddffgghhiijjkaccddffgghhiijjk"
    : "bddffhhiijjkklmmbddffhhiijjkklmm";
  const id = original + "@chromium-extension";
  const path = PathUtils.join(
    PathUtils.tempDir,
    "native-worker-root-gc-" + Services.uuid.generateUUID() + ".xpi",
  );
  const manifest = {
    manifest_version: 3,
    name: "Native Worker root GC " + type,
    version: "1.0",
    permissions: ["storage"],
    background: {
      service_worker: "worker.js",
      ...(type === "module" ? { type: "module" } : {}),
    },
    browser_specific_settings: {
      gecko: {
        id,
        floorp_chrome_extension: { schema_version: 1, original_id: original },
      },
    },
  };
  await IOUtils.write(
    path,
    zipSync(Object.fromEntries(
      Object.entries({
        "manifest.json": JSON.stringify(manifest),
        "worker.js": workerProgram,
        "control.html": '<!doctype html><script src="control.js"></script>',
        "control.js": controlProgram,
      }).map(([name, value]) => [name, new TextEncoder().encode(value)]),
    )),
  );
  let addon: NativeCorpusAddon | undefined;
  let tab: NativeCorpusTab | undefined;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "extension startup",
    );
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual native Worker",
    );
    tab = window.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await until(
      () => tab?.linkedBrowser.contentTitle === "gc-control-ready",
      "native control page",
    );
    const control = tab.linkedBrowser;
    const send = (data: object) =>
      control.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            `content.postMessage(${JSON.stringify(data)},"*")`,
          ),
        false,
      );
    const command = async <Name extends keyof WorkerRootGCResponses>(
      name: Name,
      token: string,
    ): Promise<WorkerRootGCResponses[Name]> => {
      send({ rootGCCommand: true, command: name, token });
      return await until(() => {
        const title = control.contentTitle;
        if (!title.startsWith("{")) return false;
        const value: unknown = JSON.parse(title);
        if (
          !value || typeof value !== "object" || !("token" in value) ||
          value.token !== token
        ) return false;
        return value as WorkerRootGCResponses[Name];
      }, type + " native " + name + " response");
    };
    const evidence: WorkerRootGCSnapshot[] = [];
    for (let round = 1; round <= 3; ++round) {
      const token = type + "-" + round;
      const armed = await command("arm", token + "-armed");
      assert(
        armed.armed && !armed.error,
        "The actual Worker arms a collectable weak sentinel",
      );
      control.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            `Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService).notifyObservers(null,"memory-pressure","heap-minimize");content.postMessage({gcAck:${
              JSON.stringify(token)
            }},"*");`,
          ),
        false,
      );
      await until(
        () => control.contentTitle === JSON.stringify({ gcAck: token }),
        "owned child GC request acknowledgement",
      );
      let snapshot = await command("probe", token);
      const deadline = Date.now() + 5000;
      while (!snapshot.sentinelCollected && Date.now() < deadline) {
        await new Promise((resolve) => setTimeout(resolve, 50));
        snapshot = await command("probe", token + "-retry-" + Date.now());
      }
      assert(
        !snapshot.error,
        "Actual Worker command completed without a native/API error",
      );
      assert(
        snapshot.sentinelCollected,
        "An unreachable weak sentinel was collected in the actual Worker",
      );
      assert(
        snapshot.rootAlive && snapshot.runtimeAlive && snapshot.methodAlive,
        "Native-cached root, SDK object and real function survived Worker GC",
      );
      assert(
        snapshot.sameRoot && snapshot.sameRuntime && snapshot.sameMethod,
        "The same actual native root, SDK and function remain usable",
      );
      assertEquals(
        snapshot.authoredValue,
        "retained-through-real-GC",
        "Authored root data survives GC",
      );
      assertEquals(
        snapshot.url,
        extension.baseURI.resolve("gc-proof"),
        "Detached genuine native getURL still uses the correct extension",
      );
      assertEquals(
        JSON.stringify(snapshot.storage),
        JSON.stringify({ token: snapshot.token }),
        "Real asynchronous native storage completes after GC",
      );
      evidence.push(snapshot);
    }
    console.info("WORKER_ROOT_GC_EVIDENCE", JSON.stringify({ type, evidence }));
  } finally {
    if (tab) window.gBrowser.removeTab(tab);
    await addon?.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WorkerRootGCLifetimeNative.test.mts", [
    {
      name: "classic native SDK survives actual Worker garbage collection",
      fn: () => testRootLifetime("classic"),
    },
    {
      name: "module native SDK survives actual Worker garbage collection",
      fn: () => testRootLifetime("module"),
    },
  ]);
}
