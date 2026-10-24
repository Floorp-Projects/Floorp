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
  WorkerMessageEvidence,
  WorkerMessageMode,
  WorkerMessageResult,
  WorkerScriptType,
} from "../helpers/worker-message-test-types.ts";

const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
) as { ExtensionParent: NativeCorpusExtensionParent };
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

const workerProgram = `
chrome.runtime.onMessage.addListener((message,sender,sendResponse)=>{
  if(message.mode==='promise')return Promise.resolve({mode:'promise'});
  if(message.mode==='reject')return Promise.reject(new Error('authored-worker-rejection'));
  if(message.mode==='sync'){sendResponse({mode:'sync'});return;}
  if(message.mode==='async'){setTimeout(()=>sendResponse({mode:'async'}),10);return true;}
  if(message.mode==='late'){setTimeout(()=>sendResponse({mode:'late'}),10);return;}
  if(message.mode==='object')return {ignored:true};
  return undefined;
});
chrome.runtime.onMessage.addListener(message=>message.mode==='multiple'?Promise.resolve({mode:'second-listener'}):undefined);
`;
const controlProgram = `
document.title='native-message-control-ready';
window.addEventListener('message',async event=>{
  if(!event.data?.nativeMessageProbe)return;
  const{id,mode}=event.data;
  const result=await Promise.race([
    (async()=>{
      try{
        const value=await browser.runtime.sendMessage({mode});
        return{outcome:'resolved',undefinedResult:value===undefined,value:value??null};
      }catch(error){return{outcome:'rejected',name:error.name,message:error.message};}
    })(),
    new Promise(resolve=>setTimeout(()=>resolve({outcome:'timeout'}),3000))
  ]);
  document.title=JSON.stringify({id,...result});
});`;

async function until<T>(
  read: () => T | false | undefined,
  label: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw Error("Timed out waiting for " + label);
}

function readResult(title: string, id: number): WorkerMessageResult | false {
  if (!title.startsWith("{")) return false;
  const value: unknown = JSON.parse(title);
  if (
    typeof value !== "object" || value === null || !("id" in value) ||
    value.id !== id || !("outcome" in value)
  ) return false;
  assert(
    value.outcome === "resolved" || value.outcome === "rejected" ||
      value.outcome === "timeout",
    "The authored sender reports a recognized outcome",
  );
  return value as WorkerMessageResult;
}

async function testMessages(scriptType: WorkerScriptType): Promise<void> {
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Native bindings are compiled",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "The global experimental Worker preference remains disabled",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as NativeCorpusWindow;
  const original = scriptType === "classic"
    ? "aacceeggiikkmmooaacceeggiikkmmoo"
    : "bbddffhhjjllnppobbddffhhjjllnppo";
  const id = original + "@chromium-extension";
  const manifest = {
    manifest_version: 3,
    name: "Native Worker message semantics " + scriptType,
    version: "1.0",
    background: {
      service_worker: "worker.js",
      ...(scriptType === "module" ? { type: "module" } : {}),
    },
    browser_specific_settings: {
      gecko: {
        id,
        floorp_chrome_extension: { schema_version: 1, original_id: original },
      },
    },
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    "native-worker-messages-" + Services.uuid.generateUUID() + ".xpi",
  );
  await IOUtils.write(
    path,
    zipSync(Object.fromEntries(
      Object.entries({
        "manifest.json": JSON.stringify(manifest),
        "worker.js": workerProgram,
        "control.html": '<!doctype html><script src="control.js"></script>',
        "control.js": controlProgram,
      }).map(([name, text]) => [name, new TextEncoder().encode(text)]),
    )),
  );
  const evidence: WorkerMessageEvidence[] = [];
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
      "the actual native " + scriptType + " Worker",
    );
    tab = window.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await until(
      () => tab?.linkedBrowser.contentTitle === "native-message-control-ready",
      "the genuine extension sender page",
    );
    const modes: WorkerMessageMode[] = [
      "promise",
      "reject",
      "sync",
      "async",
      "none",
      "multiple",
      "late",
      "object",
    ];
    for (const [index, mode] of modes.entries()) {
      const request = index + 1;
      tab.linkedBrowser.messageManager.loadFrameScript(
        "data:application/javascript," + encodeURIComponent(
          "content.postMessage(" +
            JSON.stringify({ nativeMessageProbe: true, id: request, mode }) +
            ',"*");',
        ),
        false,
      );
      const result = await until(
        () => tab && readResult(tab.linkedBrowser.contentTitle, request),
        "the public " + mode + " response",
      );
      evidence.push({ scriptType, mode, result });
    }
    console.log("WORKER_MESSAGE_EVIDENCE", JSON.stringify(evidence));
    for (const { mode, result } of evidence) {
      assert(result.outcome !== "timeout", mode + " settles without hanging");
      if (mode === "reject") {
        assertEquals(
          result.outcome,
          "rejected",
          "The real rejection propagates",
        );
        assertEquals(result.name, "Error", "The caller receives an Error");
        assertEquals(
          result.message,
          "authored-worker-rejection",
          "The original rejection message reaches the actual sender",
        );
      } else {
        assertEquals(result.outcome, "resolved", mode + " resolves");
        const ignored = ["none", "late", "object"].includes(mode);
        assertEquals(
          result.undefinedResult,
          ignored,
          mode + " matches Chrome's handled versus unhandled result",
        );
        assertEquals(
          JSON.stringify(result.value),
          JSON.stringify(
            ignored
              ? null
              : { mode: mode === "multiple" ? "second-listener" : mode },
          ),
          mode + " preserves the actual response payload",
        );
      }
    }
  } finally {
    if (tab) window.gBrowser.removeTab(tab);
    await addon?.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("Native Worker messaging", [
    {
      name: "Classic native Worker public message semantics",
      fn: () => testMessages("classic"),
    },
    {
      name: "Module native Worker public message semantics",
      fn: () => testMessages("module"),
    },
  ]);
}
