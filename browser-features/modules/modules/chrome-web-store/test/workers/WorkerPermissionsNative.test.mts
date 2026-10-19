// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import { invokeNativeCorpusAction } from "../../../../../../tools/chrome-extension-runtime/corpus-native-action.mjs";
import type { NativeCorpusTab } from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
import type {
  Denial,
  ManagementPermission,
  MethodShape,
  OrdinaryObject,
  PromptWindow,
  Shape,
  StorageMirror,
  WorkerExtension,
} from "../helpers/worker-permission-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
function assertOwnMethod(value: MethodShape, label: string): void {
  assert(
    value.type === "function" && value.inNamespace && value.own && value.keys &&
      value.descriptor?.valueType === "function" &&
      value.descriptor.writable && value.descriptor.enumerable &&
      value.descriptor.configurable &&
      value.descriptor.getType === "undefined" &&
      value.descriptor.setType === "undefined",
    label + " is an own writable/enumerable/configurable data function",
  );
}
async function until<T>(
  read: () =>
    | T
    | false
    | null
    | undefined
    | Promise<T | false | null | undefined>,
  label: string,
): Promise<T> {
  const end = Date.now() + 25000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw Error("Timed out waiting for " + label);
}
const program = `
const generation=crypto.randomUUID(),sdk=chrome,events=[];
let phase='ready',savedPower,savedCapture,savedRequest,savedRelease,savedGetCaptured,lastError;
const requested={permissions:['power','tabCapture']};
function methodShape(namespace,key){const descriptor=Object.getOwnPropertyDescriptor(namespace,key);return {type:typeof namespace[key],inNamespace:key in namespace,own:!!descriptor,keys:Object.keys(namespace).includes(key),descriptor:descriptor?{writable:descriptor.writable,enumerable:descriptor.enumerable,configurable:descriptor.configurable,valueType:typeof descriptor.value,getType:typeof descriptor.get,setType:typeof descriptor.set}:null};}
chrome.permissions.onAdded.addListener(value=>events.push({event:'added',permissions:value.permissions||[]}));
chrome.permissions.onRemoved.addListener(value=>events.push({event:'removed',permissions:value.permissions||[]}));
async function shape(){return {generation,sameChrome:chrome===sdk,isWorker:typeof ServiceWorkerGlobalScope!=='undefined'&&self instanceof ServiceWorkerGlobalScope,hasDocument:typeof document!=='undefined',phase,contains:await chrome.permissions.contains(requested),power:typeof chrome.power,tabCapture:typeof chrome.tabCapture,keys:Object.keys(chrome),powerIn:'power' in chrome,captureIn:'tabCapture' in chrome,samePower:savedPower?savedPower===chrome.power:null,sameCapture:savedCapture?savedCapture===chrome.tabCapture:null,sameMethods:savedRequest?savedRequest===chrome.power.requestKeepAwake&&savedRelease===chrome.power.releaseKeepAwake&&savedGetCaptured===chrome.tabCapture.getCapturedTabs:null,events,level:savedPower?.Level,captureState:savedCapture?.TabCaptureState,error:lastError};}
chrome.action.onClicked.addListener(async()=>{phase='prompting';try{const granted=await chrome.permissions.request(requested);phase=granted?'granted':'denied';if(granted){savedPower=chrome.power;savedCapture=chrome.tabCapture;savedRequest=savedPower.requestKeepAwake;savedRelease=savedPower.releaseKeepAwake;savedGetCaptured=savedCapture.getCapturedTabs;}}catch(error){phase='error';lastError=String(error);}});
chrome.runtime.onMessage.addListener(message=>{if(message.nativeOptionalPermission!==true)return;return(async()=>{
 switch(message.operation){
 case 'inspect':return shape();
 case 'managementPermission':{const namespaceType=typeof chrome.management,getSelf=methodShape(chrome.management,'getSelf'),getURL=methodShape(chrome.runtime,'getURL'),self=chrome.management.getSelf,url=chrome.runtime.getURL,privileged={getAll:methodShape(chrome.management,'getAll'),setEnabled:methodShape(chrome.management,'setEnabled'),connectNative:methodShape(chrome.runtime,'connectNative'),sendNativeMessage:methodShape(chrome.runtime,'sendNativeMessage')};return {namespaceType,selfId:(await self()).id,foreignSelfId:(await self.call({})).id,detachedURL:url('permission-fixture.txt'),foreignURL:url.call({},'permission-fixture.txt'),boundURL:chrome.runtime.getURL('permission-fixture.txt'),getSelf,getURL,privileged};}
 case 'storageMirror':{
   const local=chrome.storage.local,prototype=Object.getPrototypeOf(local);
   const calls={script:0,getter:0,proxy:0};
   const wasm=(await WebAssembly.instantiate(new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,3,2,1,0,7,5,1,1,120,0,0,10,6,1,4,0,65,1,11]))).instance.exports.x;
   const script=function(){calls.script++;};
   const proxy=new Proxy(function(){calls.proxy++;},{apply(){calls.proxy++;},get(){calls.proxy++;},getOwnPropertyDescriptor(){calls.proxy++;},getPrototypeOf(){calls.proxy++;}});
   const descriptors={floorpPoisonWasm:{value:wasm,writable:true,enumerable:true,configurable:true},floorpPoisonScript:{value:script,writable:true,enumerable:true,configurable:true},floorpPoisonProxy:{value:proxy,writable:true,enumerable:true,configurable:true},floorpPoisonNative:{value:Math.max,writable:true,enumerable:true,configurable:true},floorpPoisonGetter:{get(){calls.getter++;return script;},enumerable:true,configurable:true}};
   const keys=Object.keys(descriptors),previous=Object.fromEntries(keys.map(key=>[key,Object.getOwnPropertyDescriptor(prototype,key)])),materialized={local};
   const key='floorp-native-storage-mirror-'+generation;
   try{
     Object.defineProperties(prototype,descriptors);
     materialized.session=chrome.storage.session;
     materialized.sync=chrome.storage.sync;
     for(const [name,area] of Object.entries(materialized))await area.set({[key]:generation+':'+name});
     const areas={};
     for(const [name,area] of Object.entries(materialized))areas[name]={value:(await area.get(key))[key],get:methodShape(area,'get'),set:methodShape(area,'set'),poisonOwnKeys:keys.filter(key=>Object.hasOwn(area,key))};
     return {wasmType:typeof wasm,sharedPrototype:Object.values(materialized).every(area=>Object.getPrototypeOf(area)===prototype),calls,areas};
   }finally{
     for(const name of keys){const descriptor=previous[name];if(descriptor)Object.defineProperty(prototype,name,descriptor);else delete prototype[name];}
     await Promise.allSettled(Object.values(materialized).map(area=>area.remove(key)));
   }
 }
 case 'ordinaryObject':{let receiver=false;Object.defineProperty(chrome,'floorpFixtureGetter',{get(){receiver=this===chrome;return 'authored-getter';},configurable:true,enumerable:true});Object.defineProperty(chrome,'floorpFixtureLocked',{value:7,writable:false,enumerable:false,configurable:false});chrome.floorpFixtureOrdinary=19;const normalValue=chrome.floorpFixtureOrdinary,deleted=delete chrome.floorpFixtureOrdinary;const getterValue=chrome.floorpFixtureGetter;delete chrome.floorpFixtureGetter;const descriptor=Object.getOwnPropertyDescriptor(chrome,'floorpFixtureLocked');return {plainPrototype:Object.getPrototypeOf(chrome)===Object.prototype,tag:Object.prototype.toString.call(chrome),getterValue,getterReceiver:receiver,lockedValue:chrome.floorpFixtureLocked,lockedFlags:descriptor.writable===false&&descriptor.enumerable===false&&descriptor.configurable===false&&!Object.keys(chrome).includes('floorpFixtureLocked'),setLocked:Reflect.set(chrome,'floorpFixtureLocked',8),deleteLocked:Reflect.deleteProperty(chrome,'floorpFixtureLocked'),normalValue,deleted,unknownType:typeof chrome.floorpUnknownNamespace,thenType:typeof chrome.then,constructorIsObject:chrome.constructor===Object};}
 case 'apiDescriptors':return {requestKeepAwake:methodShape(savedPower,'requestKeepAwake'),releaseKeepAwake:methodShape(savedPower,'releaseKeepAwake'),getCapturedTabs:methodShape(savedCapture,'getCapturedTabs')};
 case 'powerRequest':return {type:typeof savedRequest('system')};
 case 'powerRelease':return {type:typeof savedRelease()};
 case 'capture':return {tabs:await savedGetCaptured()};
 case 'remove':return chrome.permissions.remove(requested);
 case 'revoked':{const results={};for(const [name,call] of Object.entries({requestKeepAwake:()=>savedRequest('system'),releaseKeepAwake:()=>savedRelease(),getCapturedTabs:()=>savedGetCaptured()})){try{const result=call();if(result&&typeof result.catch==='function')result.catch(()=>{});results[name]={synchronous:false};}catch(error){results[name]={synchronous:true,name:error.name,message:error.message};}}return results;}
 }
})();});`;
const controlProgram =
  `document.title='native-permission-control-ready';window.addEventListener('message',async event=>{const value=event.data;if(value?.nativeOptionalPermission!==true)return;try{const result=await browser.runtime.sendMessage(value);document.title=JSON.stringify({request:value.request,result});}catch(error){document.title=JSON.stringify({request:value.request,error:String(error)});}});`;
async function allowNativePrompt(
  window: PromptWindow,
  name: string,
): Promise<void> {
  const notification = await until(() => {
    const current = window.PopupNotifications.getNotification(
      "addon-webext-permissions",
    );
    if (
      current?.options.name !== name ||
      window.PopupNotifications.panel.state !== "open"
    ) return false;
    return window.PopupNotifications.panel.querySelector(
      "popupnotification[popupid='addon-webext-permissions']",
    );
  }, "actual optional power/tabCapture permission doorhanger");
  const node = notification as Element & { button?: HTMLElement };
  const button = node.button ||
    node.shadowRoot?.querySelector(".popup-notification-primary-button") ||
    node.querySelector(".popup-notification-primary-button");
  assert(button, "real native Allow control exists");
  const rect = button.getBoundingClientRect();
  assert(
    rect.width > 0 && rect.height > 0,
    "real native Allow control is visible",
  );
  const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
  let trusted = false;
  const observe = (event: Event) => {
    if (event.isTrusted) trusted = true;
  };
  button.addEventListener("click", observe);
  try {
    window.synthesizeMouseEvent("mousedown", x, y, {
      button: 0,
      buttons: 1,
      clickCount: 1,
    });
    window.synthesizeMouseEvent("mouseup", x, y, {
      button: 0,
      buttons: 0,
      clickCount: 1,
    });
    await until(() => trusted, "trusted native permission Allow click");
  } finally {
    button.removeEventListener("click", observe);
  }
}
async function fixture(type: "classic" | "module" | "ordinary"): Promise<void> {
  const worker = type !== "ordinary";
  const gfx = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
  assert(
    !gfx.isHeadless,
    "actual native permission consent requires a headed browser; headless execution cannot qualify",
  );
  if (worker) {
    assert(
      AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
      "compiled native extension WebIDL bindings are required; document fallback cannot satisfy this test",
    );
  }
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as {
    ExtensionParent: {
      GlobalManager: { extensionMap: Map<string, WorkerExtension> };
    };
  };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as PromptWindow;
  const original = type === "classic"
    ? "abcdefghijklmnopabcdefghijklmnop"
    : "ponmlkjihgfedcbaponmlkjihgfedcba";
  const id = worker
    ? original + "@chromium-extension"
    : "ordinary-optional-permission@floorp.test";
  const name = "Native optional permission " + type;
  const manifest = {
    manifest_version: 3,
    name,
    version: "1.0",
    permissions: worker ? ["storage"] : ["power", "tabCapture"],
    optional_permissions: worker ? ["power", "tabCapture"] : [],
    action: { default_title: name },
    background: worker
      ? {
        service_worker: "background.js",
        ...(type === "module" ? { type: "module" } : {}),
      }
      : { scripts: ["background.js"] },
    ...(worker
      ? {
        content_security_policy: {
          extension_pages:
            "script-src 'self' 'wasm-unsafe-eval'; object-src 'self'",
        },
      }
      : {}),
    browser_specific_settings: {
      gecko: {
        id,
        ...(worker
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
  const ordinaryProgram =
    `document.title=JSON.stringify({power:typeof chrome.power,tabCapture:typeof chrome.tabCapture,powerIn:'power' in chrome,captureIn:'tabCapture' in chrome,keys:Object.keys(chrome)});`;
  const path = PathUtils.join(
    PathUtils.tempDir,
    "floorp-worker-permission-" + Services.uuid.generateUUID() + ".xpi",
  );
  await IOUtils.write(
    path,
    zipSync(Object.fromEntries(
      Object.entries({
        "manifest.json": JSON.stringify(manifest),
        "background.js": worker ? program : ordinaryProgram,
        "control.html": '<!doctype html><script src="control.js"></script>',
        "control.js": controlProgram,
      }).map(([file, value]) => [file, new TextEncoder().encode(value)]),
    )),
  );
  let addon: { uninstall(): Promise<void> } | undefined,
    control: NativeCorpusTab | undefined,
    target: NativeCorpusTab | undefined;
  let stopObserver: (() => void) | undefined;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "installed extension policy",
    );
    if (!worker) {
      const background = await until(
        () => extension.backgroundContext?.xulBrowser,
        "ordinary Firefox document background",
      );
      const raw = await until(
        () =>
          background.contentTitle.startsWith("{") && background.contentTitle,
        "ordinary Firefox namespace probe",
      );
      const result = JSON.parse(raw) as {
        power: string;
        tabCapture: string;
        powerIn: boolean;
        captureIn: boolean;
        keys: string[];
      };
      assertEquals(
        result.power,
        "undefined",
        "ordinary Firefox power namespace remains absent",
      );
      assertEquals(
        result.tabCapture,
        "undefined",
        "ordinary Firefox tabCapture namespace remains absent",
      );
      assert(
        !result.powerIn && !result.captureIn &&
          !result.keys.includes("power") && !result.keys.includes("tabCapture"),
        "ordinary Firefox namespace membership remains absent",
      );
      return;
    }
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual native extension Service Worker",
    );
    control = window.gBrowser.addTab(
      extension.baseURI.resolve("control.html"),
      { triggeringPrincipal: extension.principal },
    );
    const controlBrowser = control.linkedBrowser;
    await until(
      () => controlBrowser.contentTitle === "native-permission-control-ready",
      "real extension control page",
    );
    let sequence = 0;
    const command = async <T,>(operation: string): Promise<T> => {
      const request = ++sequence;
      controlBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            "content.postMessage(" +
              JSON.stringify({
                nativeOptionalPermission: true,
                operation,
                request,
              }) + ',"*");',
          ),
        false,
      );
      const response = await until(() => {
        try {
          const value = JSON.parse(controlBrowser.contentTitle) as {
            request: number;
            result: T;
            error?: string;
          };
          return value.request === request && value;
        } catch {
          return false;
        }
      }, "native Worker " + operation);
      if (response.error) throw Error(response.error);
      return response.result;
    };
    const initial = await command<Shape>("inspect");
    assert(
      initial.isWorker && !initial.hasDocument && initial.sameChrome,
      "probe runs in a native Worker with stable Chrome object",
    );
    assertEquals(initial.contains, false, "optional pair starts ungranted");
    assert(
      initial.power === "undefined" && initial.tabCapture === "undefined" &&
        !initial.powerIn && !initial.captureIn &&
        !initial.keys.includes("power") && !initial.keys.includes("tabCapture"),
      "ungranted namespaces are absent from values, keys and in checks",
    );
    const management = await command<ManagementPermission>(
      "managementPermission",
    );
    assertEquals(
      management.namespaceType,
      "object",
      "public management namespace is available without management permission",
    );
    assert(
      management.selfId === id || management.selfId === original,
      "public management.getSelf returns the installed caller's own identity",
    );
    assertOwnMethod(management.getSelf, "public management.getSelf");
    assertOwnMethod(management.getURL, "public runtime.getURL");
    assertEquals(
      management.foreignSelfId,
      management.selfId,
      "public management.getSelf ignores a foreign caller-supplied this value, matching Chrome",
    );
    assertEquals(
      management.detachedURL,
      management.boundURL,
      "detached runtime.getURL uses the same real SDK receiver as a normal call",
    );
    assertEquals(
      management.foreignURL,
      management.boundURL,
      "public runtime.getURL ignores a foreign caller-supplied this value, matching Chrome",
    );
    assertEquals(
      management.detachedURL,
      extension.baseURI.resolve("permission-fixture.txt"),
      "detached runtime.getURL returns the actual installed resource URL",
    );
    for (const [method, value] of Object.entries(management.privileged)) {
      assert(
        value.type === "undefined" && !value.inNamespace && !value.own &&
          !value.keys && value.descriptor === null,
        "ungranted " + method +
          " is absent from values, membership, own keys and descriptors, matching the genuine Chrome member oracle",
      );
    }
    const storageMirror = await command<StorageMirror>("storageMirror");
    assertEquals(
      storageMirror.wasmType,
      "function",
      "prototype poisoning includes a genuine compiled WebAssembly export",
    );
    assert(
      storageMirror.sharedPrototype,
      "first local area and subsequently materialized session/sync areas share the poisoned SDK prototype",
    );
    assert(
      Object.values(storageMirror.calls).every((count) => count === 0),
      "mirroring real storage areas never invokes authored prototype getters, script functions or callable Proxy traps",
    );
    for (const [name, area] of Object.entries(storageMirror.areas)) {
      assertOwnMethod(area.get, "materialized storage." + name + ".get");
      assertOwnMethod(area.set, "materialized storage." + name + ".set");
      assertEquals(
        area.value,
        initial.generation + ":" + name,
        "real storage." + name +
          " get/set preserve independent area state through the native SDK forwarder",
      );
      assertEquals(
        JSON.stringify(area.poisonOwnKeys),
        "[]",
        "native mirroring does not copy authored prototype poison into storage." +
          name + " own properties",
      );
    }
    const ordinaryObject = await command<OrdinaryObject>("ordinaryObject");
    assert(
      ordinaryObject.plainPrototype && ordinaryObject.tag === "[object Object]",
      "native Chrome root has the ordinary Object prototype and brand",
    );
    assert(
      ordinaryObject.getterValue === "authored-getter" &&
        ordinaryObject.getterReceiver,
      "authored getter descriptors execute with the Chrome root as receiver",
    );
    assert(
      ordinaryObject.lockedFlags && ordinaryObject.lockedValue === 7 &&
        !ordinaryObject.setLocked && !ordinaryObject.deleteLocked,
      "authored nonwritable/nonenumerable/nonconfigurable data descriptors are honored",
    );
    assert(
      ordinaryObject.normalValue === 19 && ordinaryObject.deleted,
      "ordinary Chrome root assignment and deletion work",
    );
    assert(
      ordinaryObject.unknownType === "undefined" &&
        ordinaryObject.thenType === "undefined" &&
        ordinaryObject.constructorIsObject,
      "unknown, then and constructor use ordinary property semantics without invented APIs",
    );
    target = window.gBrowser.addTab(
      "data:text/html,<!doctype html><title>Native optional permission action target</title>",
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    let observedPair = false;
    const observer = {
      observe(subject: nsISupports) {
        const prompt = (subject as unknown as {
          wrappedJSObject: {
            name: string;
            permissions: { permissions: string[] };
          };
        }).wrappedJSObject;
        if (prompt.name === name) {
          assertEquals(
            JSON.stringify([...prompt.permissions.permissions].sort()),
            JSON.stringify(["power", "tabCapture"].sort()),
            "real native request contains exactly the optional pair",
          );
          observedPair = true;
        }
      },
    };
    Services.obs.addObserver(
      observer,
      "webextension-optional-permission-prompt",
    );
    stopObserver = () =>
      Services.obs.removeObserver(
        observer,
        "webextension-optional-permission-prompt",
      );
    const action = await invokeNativeCorpusAction(extension, window, target);
    await until(
      () => observedPair,
      "actual optional permission request observer",
    );
    const beforeConsent = await command<Shape>("inspect");
    assert(
      beforeConsent.phase === "prompting" && !beforeConsent.contains &&
        beforeConsent.power === "undefined" &&
        beforeConsent.tabCapture === "undefined",
      "optional APIs remain unavailable while the real consent decision is pending",
    );
    await allowNativePrompt(window, name);
    const granted = await until(async () => {
      const value = await command<Shape>("inspect");
      if (value.error) throw Error(value.error);
      return value.phase === "granted" && value.power === "object" &&
        value.tabCapture === "object" && value;
    }, "same native Worker optional API exposure after real consent");
    assertEquals(
      granted.generation,
      initial.generation,
      "permission grant keeps the same Worker generation",
    );
    assert(
      granted.sameChrome && granted.samePower && granted.sameCapture &&
        granted.sameMethods &&
        granted.contains,
      "same Chrome object exposes granted real API references",
    );
    assert(
      granted.powerIn && granted.captureIn && granted.keys.includes("power") &&
        granted.keys.includes("tabCapture"),
      "granted namespaces appear in membership and own keys",
    );
    assert(
      granted.events.some((event) =>
        event.event === "added" && event.permissions.includes("power") &&
        event.permissions.includes("tabCapture")
      ),
      "real permissions.onAdded event delivered",
    );
    const apiDescriptors = await command<Record<string, MethodShape>>(
      "apiDescriptors",
    );
    for (const [name, descriptor] of Object.entries(apiDescriptors)) {
      assertOwnMethod(descriptor, "granted " + name);
    }
    const power = Cc["@mozilla.org/power/powermanagerservice;1"].getService(
      Ci.nsIPowerManagerService,
    ) as nsIPowerManagerService & { newWakeLockForChromeExtension?: unknown };
    assert(
      typeof power.newWakeLockForChromeExtension === "function",
      "compiled native Chrome power backend is required",
    );
    assertEquals(
      power.getWakeLockState("cpu"),
      "unlocked",
      "native wake-lock baseline is isolated",
    );
    assertEquals(
      (await command<{ type: string }>("powerRequest")).type,
      "undefined",
      "detached granted power API calls the real backend and returns synchronous void",
    );
    await until(
      () => power.getWakeLockState("cpu") !== "unlocked",
      "granted Worker acquired real native CPU wake lock",
    );
    assertEquals(
      (await command<{ type: string }>("powerRelease")).type,
      "undefined",
      "detached granted power release returns synchronous void",
    );
    await until(
      () => power.getWakeLockState("cpu") === "unlocked",
      "granted Worker released real native CPU wake lock",
    );
    const captured = await command<{ tabs: unknown[] }>("capture");
    assert(
      Array.isArray(captured.tabs),
      "detached granted tabCapture method calls the real backend and resolves captured tabs",
    );
    assertEquals(
      await command<boolean>("remove"),
      true,
      "actual permissions.remove revokes the granted pair",
    );
    const revoked = await until(async () => {
      const value = await command<Shape>("inspect");
      return !value.contains && value.events.some((event) =>
        event.event === "removed"
      ) && value;
    }, "same Worker permission removal event");
    assertEquals(
      revoked.generation,
      initial.generation,
      "permission removal keeps the same Worker generation",
    );
    assert(
      revoked.sameChrome && revoked.samePower && revoked.sameCapture &&
        revoked.sameMethods &&
        revoked.power === "object" && revoked.tabCapture === "object",
      "existing SDK namespace references remain stable after revocation, matching Chrome",
    );
    assertEquals(
      revoked.level?.SYSTEM,
      "system",
      "retained power enum stays readable without API privileges",
    );
    assertEquals(
      revoked.captureState?.ACTIVE,
      "active",
      "retained tabCapture enum stays readable without API privileges",
    );
    const denials = await command<Record<string, Denial>>("revoked");
    for (
      const method of [
        "requestKeepAwake",
        "releaseKeepAwake",
        "getCapturedTabs",
      ]
    ) {
      const denial = denials[method];
      assert(
        denial.synchronous && denial.name === "Error" &&
          /not available|permission|privilege|not allowed/i.test(
            denial.message || "",
          ),
        "revoked saved " + method +
          " reference throws a synchronous permission Error",
      );
    }
    assertEquals(
      power.getWakeLockState("cpu"),
      "unlocked",
      "revoked calls cannot acquire a native wake lock",
    );
    await extension.terminateBackground();
    await until(
      () => extension.backgroundState === "stopped",
      "actual native Worker termination",
    );
    const restarted = await command<Shape>("inspect");
    assert(
      restarted.generation !== initial.generation && restarted.isWorker &&
        !restarted.hasDocument,
      "runtime message wakes a fresh native Worker generation",
    );
    assert(
      !restarted.contains && restarted.power === "undefined" &&
        restarted.tabCapture === "undefined" && !restarted.powerIn &&
        !restarted.captureIn && !restarted.keys.includes("power") &&
        !restarted.keys.includes("tabCapture"),
      "revoked optional namespaces are absent in the restarted Worker",
    );
    console.log(
      "WORKER_PERMISSION_EVIDENCE=" +
        JSON.stringify({
          type,
          initial,
          management,
          storageMirror,
          ordinaryObject,
          beforeConsent,
          headed: !gfx.isHeadless,
          action,
          granted,
          apiDescriptors,
          revoked,
          denials,
          restarted,
        }),
    );
  } finally {
    stopObserver?.();
    if (target) window.gBrowser.removeTab(target, { skipPermitUnload: true });
    if (control) window.gBrowser.removeTab(control, { skipPermitUnload: true });
    await addon?.uninstall();
    await IOUtils.remove(path);
  }
}
export async function runAllTests() {
  await runTests("WorkerPermissionsNative", [{
    name: "classic native Worker real optional permission grant/revoke/restart",
    fn: () => fixture("classic"),
  }, {
    name: "module native Worker real optional permission grant/revoke/restart",
    fn: () => fixture("module"),
  }, {
    name: "ordinary Firefox document keeps Chrome-only namespaces absent",
    fn: () => fixture("ordinary"),
  }]);
}
