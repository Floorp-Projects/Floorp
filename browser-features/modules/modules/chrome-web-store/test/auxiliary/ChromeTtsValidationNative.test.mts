// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Empty utterances plus an unavailable voice prevent speech even on failure.
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type { ComponentAddonManager } from "../helpers/capture-component-test-types.ts";
import type {
  TtsValidationParent,
  TtsValidationReply,
  TtsValidationWindow,
} from "../helpers/tts-validation-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until<Value>(
  read: () => Value | false | undefined,
  description: string,
): Promise<Value> {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw Error("Timed out waiting for " + description);
}
const probeSource = `
async function invalidRanges(context){
 const before=await browser.tts.isSpeaking(),events=[],invalid=[];
 for(const [field,value] of [['volume',-Number.EPSILON],['volume',1+Number.EPSILON],['pitch',-Number.EPSILON],['pitch',2.000001],['rate',10.000001]]){
  let rejection;
  try{await browser.tts.speak('',{[field]:value,voiceName:'floorp-nonexistent-range-validation-voice',onEvent:event=>events.push(event)});}
  catch(error){rejection=error;}
  if(!rejection||rejection.message!=='Invalid speech '+field+'.')throw Error('Expected exact parent range rejection for '+field+', received '+String(rejection));
  invalid.push({field,value:String(value),name:rejection.name,message:rejection.message});
 }
 await new Promise(resolve=>setTimeout(resolve,50));
 return {context,isWorker:typeof document==='undefined',before,after:await browser.tts.isSpeaking(),eventCount:events.length,invalid};
}
`;
async function validateTts(
  mode: "document" | "worker" | "ordinary",
): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: ComponentAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: TtsValidationParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "actual native Worker SDK is compiled",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as TtsValidationWindow;
  const original = mode === "worker"
    ? "ccccccccccccccccdddddddddddddddd"
    : "eeeeeeeeeeeeeeeeffffffffffffffff";
  const converted = mode !== "ordinary",
    id = converted
      ? original + "@chromium-extension"
      : "ordinary-tts-validation@example.test";
  const manifest = {
    manifest_version: 3,
    name: "Native TTS validation " + mode,
    version: "1",
    permissions: ["tts"],
    ...(converted ? { background: { service_worker: "worker.js" } } : {}),
    browser_specific_settings: {
      gecko: {
        id,
        ...(converted
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
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "worker.js": probeSource +
      `browser.runtime.onMessage.addListener(message=>message.target==='tts-range-validation'?invalidRanges('worker'):undefined);`,
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js": probeSource +
      `window.addEventListener('message',async event=>{if(event.data?.target!=='tts-range-validation')return;const requestId=event.data.requestId;try{if(event.data.mode==='ordinary'){document.title=JSON.stringify({requestId,namespace:typeof browser.tts});return;}const result=event.data.mode==='worker'?await browser.runtime.sendMessage(event.data):await invalidRanges('document');document.title=JSON.stringify({requestId,result});}catch(error){document.title=JSON.stringify({requestId,error:String(error)});}});document.title='tts-validation-ready';`,
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    "tts-validation-" + Services.uuid.generateUUID() + ".xpi",
  );
  await IOUtils.write(
    path,
    zipSync(
      Object.fromEntries(
        Object.entries(files).map((
          [name, source],
        ) => [name, new TextEncoder().encode(source)]),
      ),
    ),
  );
  const addon = await AddonManager.installTemporaryAddon(
    new FileUtils.File(path),
  );
  let tab: ReturnType<TtsValidationWindow["gBrowser"]["addTab"]> | undefined;
  try {
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "installed TTS fixture",
    );
    if (converted) {
      await until(
        () =>
          extension.backgroundState === "running" &&
          extension.backgroundContext?.viewType === "background_worker",
        "real native Worker startup",
      );
    }
    tab = window.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    const browser = tab.linkedBrowser;
    await until(
      () => browser.contentTitle === "tts-validation-ready",
      "actual document listener",
    );
    const requestId = String(Services.uuid.generateUUID());
    browser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          "content.postMessage(" +
            JSON.stringify({
              target: "tts-range-validation",
              mode,
              requestId,
            }) + ',"*");',
        ),
      false,
    );
    const reply = await until(() => {
      if (!browser.contentTitle.startsWith("{")) return false;
      const result = JSON.parse(browser.contentTitle) as TtsValidationReply;
      if (result.requestId !== requestId) return false;
      if (result.error) throw Error(result.error);
      return result;
    }, "correlated public TTS validation result");
    if (mode === "ordinary") {
      assertEquals(
        reply.namespace,
        "undefined",
        "ordinary Firefox extension has no Chrome TTS namespace",
      );
      return;
    }
    const result = reply.result;
    assert(result, "actual API result was returned");
    assertEquals(result.context, mode, "the intended real API context ran");
    assertEquals(
      result.isWorker,
      mode === "worker",
      "Worker case runs without a document",
    );
    assertEquals(result.before, false, "native speech starts idle");
    assertEquals(
      result.after,
      false,
      "invalid empty utterances never start speech",
    );
    assertEquals(
      result.eventCount,
      0,
      "rejected utterances never dispatch speech events",
    );
    assertEquals(
      JSON.stringify(result.invalid.map((item) => item.field)),
      JSON.stringify(["volume", "volume", "pitch", "pitch", "rate"]),
      "all exact field-specific range errors were observed",
    );
    for (const item of result.invalid) {
      assertEquals(
        item.message,
        "Invalid speech " + item.field + ".",
        "rejection came from range validation before voice lookup",
      );
    }
    console.log(
      "[ChromeTtsValidationNative]",
      JSON.stringify({ mode, id, result }),
    );
  } finally {
    try {
      await addon.uninstall();
    } finally {
      if (tab?.isConnected) window.gBrowser.removeTab(tab);
      await IOUtils.remove(path, { ignoreAbsent: true });
    }
  }
}
export function runAllTests(): Promise<void> {
  return runTests("ChromeTtsValidationNative.test.mts", [
    {
      name:
        "document TTS rejects invalid ranges before speech or voice selection",
      fn: () => validateTts("document"),
    },
    {
      name:
        "native Worker TTS rejects invalid ranges before speech or voice selection",
      fn: () => validateTts("worker"),
    },
    {
      name: "ordinary Firefox extension retains its TTS namespace boundary",
      fn: () => validateTts("ordinary"),
    },
  ]);
}
