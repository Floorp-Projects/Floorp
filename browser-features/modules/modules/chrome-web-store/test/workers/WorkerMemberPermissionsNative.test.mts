// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import { invokeNativeCorpusAction } from "../../../../../../tools/chrome-extension-runtime/corpus-native-action.mjs";
import type {
  NativeCorpusExtensionParent,
  NativeCorpusTab,
} from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
import type {
  MethodShape,
  PermissionClock,
} from "../helpers/worker-permission-test-types.ts";
import type {
  MemberMode,
  MemberPromptWindow,
  MemberRead,
  MemberRevocation,
  MemberState,
} from "../helpers/worker-member-permission-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function until<T>(
  read: () =>
    | T
    | false
    | null
    | undefined
    | Promise<T | false | null | undefined>,
  label: string,
): Promise<T> {
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw Error("Timed out waiting for " + label);
}
function ownFunction(shape: MethodShape, label: string): void {
  assert(
    shape.type === "function" && shape.inNamespace && shape.own && shape.keys &&
      shape.descriptor?.valueType === "function" && shape.descriptor.writable &&
      shape.descriptor.enumerable && shape.descriptor.configurable &&
      shape.descriptor.getType === "undefined" &&
      shape.descriptor.setType === "undefined",
    label + " is an own writable/enumerable/configurable data function",
  );
}
function absent(shape: MethodShape, label: string): void {
  assert(
    shape.type === "undefined" && !shape.inNamespace && !shape.own &&
      !shape.keys && shape.descriptor === null,
    label +
      " remains absent after pre-grant namespace materialization, matching Chrome",
  );
}
function background(mode: MemberMode): string {
  return `
const mode=${
    JSON.stringify(mode)
  },generation=crypto.randomUUID(),sdk=chrome,requested={permissions:['management']},events=[];
let phase='ready',lastError,savedManagement,savedGetAll,savedGetSelf,preManagement;
function methodShape(namespace,key){const value=namespace[key],descriptor=Object.getOwnPropertyDescriptor(namespace,key);return {type:typeof value,inNamespace:key in namespace,own:!!descriptor,keys:Object.keys(namespace).includes(key),descriptor:descriptor?{valueType:typeof descriptor.value,writable:descriptor.writable,enumerable:descriptor.enumerable,configurable:descriptor.configurable,getType:typeof descriptor.get,setType:typeof descriptor.set}:null};}
const preAccess={mode};
if(mode==='enumeration'){preAccess.keys=Object.keys(sdk);preAccess.ownKeys=Reflect.ownKeys(sdk).filter(key=>typeof key==='string');}
if(mode==='descriptor'){const descriptor=Object.getOwnPropertyDescriptor(sdk,'management');preManagement=descriptor?.value;preAccess.descriptor=descriptor?{valueType:typeof descriptor.value,writable:descriptor.writable,enumerable:descriptor.enumerable,configurable:descriptor.configurable,getType:typeof descriptor.get,setType:typeof descriptor.set}:null;if(preManagement){preAccess.getSelf=methodShape(preManagement,'getSelf');preAccess.getAll=methodShape(preManagement,'getAll');}}
chrome.permissions.onAdded.addListener(value=>events.push({event:'added',permissions:value.permissions||[]}));
chrome.permissions.onRemoved.addListener(value=>events.push({event:'removed',permissions:value.permissions||[]}));
chrome.action.onClicked.addListener(async()=>{phase='prompting';try{phase=await chrome.permissions.request(requested)?'granted':'denied';}catch(error){lastError=String(error);phase='error';}});
chrome.runtime.onMessage.addListener(message=>{if(message.floorpMemberPermission!==true)return;return(async()=>{
switch(message.operation){
case 'state':if(preManagement&&!preAccess.selfId)preAccess.selfId=(await preManagement.getSelf()).id;return {generation,sameChrome:chrome===sdk,isWorker:typeof ServiceWorkerGlobalScope!=='undefined'&&self instanceof ServiceWorkerGlobalScope,hasDocument:typeof document!=='undefined',phase,contains:await chrome.permissions.contains(requested),preAccess,events,error:lastError};
case 'firstRead':{savedManagement=chrome.management;savedGetSelf=savedManagement.getSelf;savedGetAll=savedManagement.getAll;const selfInfo=await savedGetSelf(),all=typeof savedGetAll==='function'?await savedGetAll():null;return {generation,sameChrome:chrome===sdk,namespaceType:typeof savedManagement,samePreManagement:preManagement?preManagement===savedManagement:null,getSelf:methodShape(savedManagement,'getSelf'),getAll:methodShape(savedManagement,'getAll'),setEnabled:methodShape(savedManagement,'setEnabled'),selfId:selfInfo.id,listContainsSelf:all?all.some(item=>item.id===selfInfo.id):null,extensionCount:all?all.length:null};}
case 'remove':return chrome.permissions.remove(requested);
case 'revoked':{let denial=null;if(typeof savedGetAll==='function'){try{const value=savedGetAll();try{await value;denial={synchronous:false,succeeded:true};}catch(error){denial={synchronous:false,succeeded:false,name:error.name,message:error.message};}}catch(error){denial={synchronous:true,succeeded:false,name:error.name,message:error.message};}}return {generation,sameChrome:chrome===sdk,sameManagement:chrome.management===savedManagement,sameGetAll:chrome.management.getAll===savedGetAll,getAll:methodShape(savedManagement,'getAll'),selfId:(await savedGetSelf()).id,denial};}
default:throw new Error('Unknown authored member permission command');
}})();});`;
}
const controlProgram =
  `document.title='native-member-control-ready';window.addEventListener('message',async event=>{const value=event.data;if(value?.floorpMemberPermission!==true)return;try{document.title=JSON.stringify({request:value.request,result:await browser.runtime.sendMessage(value)});}catch(error){document.title=JSON.stringify({request:value.request,error:String(error)});}});`;

async function allowPrompt(
  window: MemberPromptWindow,
  name: string,
): Promise<void> {
  const notification = await until(() => {
    const current = window.PopupNotifications.getNotification(
      "addon-webext-permissions",
    );
    const node = window.PopupNotifications.panel.querySelector(
      "popupnotification[popupid='addon-webext-permissions']",
    );
    if (
      current?.options.name !== name ||
      window.PopupNotifications.panel.state !== "open" || !node?.isConnected ||
      Services.focus.activeWindow !== window ||
      node.matches(":-moz-window-inactive") ||
      !Number.isFinite(current.timeShown) ||
      (ChromeUtils as PermissionClock).now() - current.timeShown <
        Math.max(
            0,
            Services.prefs.getIntPref("security.notification_enable_delay"),
          ) + 100
    ) return false;
    return node;
  }, "actual management permission doorhanger");
  const node = notification as Element & { button?: HTMLElement };
  const button = node.button ||
    node.shadowRoot?.querySelector(".popup-notification-primary-button") ||
    node.querySelector(".popup-notification-primary-button");
  assert(button, "native management Allow button exists");
  const rect = button.getBoundingClientRect();
  assert(
    rect.width > 0 && rect.height > 0,
    "native management Allow is visible",
  );
  let trusted = false;
  const observe = (event: Event) => {
    if (event.isTrusted) trusted = true;
  };
  button.addEventListener("click", observe);
  try {
    const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
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
    await until(() => trusted, "trusted native management Allow click");
  } finally {
    button.removeEventListener("click", observe);
  }
}

async function fixture(
  kind: "classic" | "module",
  mode: MemberMode,
): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "compiled native Worker bindings are required",
  );
  assert(
    !Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo).isHeadless,
    "real management consent requires a headed browser",
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
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as MemberPromptWindow;
  assert(window, "actual browser window exists");
  const index = ["unread", "enumeration", "descriptor"].indexOf(mode) +
    (kind === "module" ? 3 : 0);
  const original = "a".repeat(31) + String.fromCharCode(98 + index),
    id = original + "@chromium-extension";
  const name = `Native management member ${kind} ${mode}`;
  const manifest = {
    manifest_version: 3,
    name,
    version: "1.0",
    optional_permissions: ["management"],
    action: { default_title: name },
    background: {
      service_worker: "background.js",
      ...(kind === "module" ? { type: "module" } : {}),
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
    "floorp-worker-member-" + Services.uuid.generateUUID() + ".xpi",
  );
  await IOUtils.write(
    path,
    zipSync(Object.fromEntries(
      Object.entries({
        "manifest.json": JSON.stringify(manifest),
        "background.js": background(mode),
        "control.html": '<!doctype html><script src="control.js"></script>',
        "control.js": controlProgram,
      }).map(([name, value]) => [name, new TextEncoder().encode(value)]),
    )),
  );
  let addon: { uninstall(): Promise<void> } | undefined,
    control: NativeCorpusTab | undefined,
    target: NativeCorpusTab | undefined;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "actual authored management extension",
    );
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual native service worker, no document fallback",
    );
    control = window.gBrowser.addTab(
      extension.baseURI.resolve("control.html"),
      { triggeringPrincipal: extension.principal },
    );
    const browser = control.linkedBrowser;
    await until(
      () => browser.contentTitle === "native-member-control-ready",
      "actual extension control page",
    );
    let sequence = 0;
    const command = async <T,>(operation: string): Promise<T> => {
      const request = ++sequence;
      browser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            "content.postMessage(" +
              JSON.stringify({
                floorpMemberPermission: true,
                operation,
                request,
              }) + ',"*");',
          ),
        false,
      );
      const response = await until(() => {
        try {
          const value = JSON.parse(browser.contentTitle) as {
            request: number;
            result: T;
            error?: string;
          };
          return value.request === request && value;
        } catch {
          return false;
        }
      }, "Worker member " + operation);
      if (response.error) throw Error(response.error);
      return response.result;
    };
    const initial = await command<MemberState>("state");
    assert(
      initial.isWorker && !initial.hasDocument && initial.sameChrome &&
        !initial.contains,
      "fresh real Worker starts with ungranted management permission",
    );
    if (mode === "enumeration") {
      assert(
        initial.preAccess.keys?.includes("management") &&
          initial.preAccess.ownKeys?.includes("management"),
        "both root enumerations expose public namespace name without constructing it",
      );
    } else if (mode === "descriptor") {
      const descriptor = initial.preAccess.descriptor;
      assert(
        descriptor?.valueType === "object" && descriptor.writable &&
          descriptor.enumerable && descriptor.configurable &&
          descriptor.getType === "undefined" &&
          descriptor.setType === "undefined",
        "root descriptor is own data object and materializes management",
      );
      assert(
        initial.preAccess.getSelf && initial.preAccess.getAll,
        "descriptor control captured actual public members",
      );
      ownFunction(initial.preAccess.getSelf, "ungranted public getSelf");
      absent(initial.preAccess.getAll, "ungranted getAll");
      assert(
        initial.preAccess.selfId === original ||
          initial.preAccess.selfId === id,
        "ungranted public getSelf returns actual caller",
      );
    }
    target = window.gBrowser.addTab(
      "data:text/html,<!doctype html><title>Native management permission action target</title>",
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    await until(
      () =>
        target?.linkedBrowser.contentTitle ===
          "Native management permission action target",
      "fully loaded action target before real consent",
    );
    const action = await invokeNativeCorpusAction(extension, window, target);
    const beforeConsent = await until(async () => {
      const state = await command<MemberState>("state");
      if (state.error) throw Error(state.error);
      return state.phase === "prompting" && state;
    }, "management request pending real consent");
    assert(
      !beforeConsent.contains,
      "management remains ungranted before native Allow",
    );
    await allowPrompt(window, name);
    const granted = await until(async () => {
      const state = await command<MemberState>("state");
      if (state.error) throw Error(state.error);
      return state.phase === "granted" && state.contains && state;
    }, "real optional management grant");
    assertEquals(
      granted.generation,
      initial.generation,
      "grant preserves same Worker generation",
    );
    assert(
      granted.sameChrome &&
        granted.events.some((event) =>
          event.event === "added" && event.permissions.includes("management")
        ),
      "same Chrome receives actual management onAdded",
    );
    const first = await command<MemberRead>("firstRead");
    assertEquals(
      first.generation,
      initial.generation,
      "first namespace read remains in same Worker",
    );
    assert(
      first.sameChrome && first.namespaceType === "object",
      "first management read returns real namespace",
    );
    ownFunction(first.getSelf, "public getSelf after grant");
    if (mode === "descriptor") {
      assert(
        first.samePreManagement,
        "pre-grant descriptor namespace retains identity",
      );
      absent(first.getAll, "descriptor-frozen getAll");
      absent(first.setEnabled, "descriptor-frozen setEnabled");
      assertEquals(
        first.listContainsSelf,
        null,
        "control never invokes an unavailable private method",
      );
    } else {
      ownFunction(first.getAll, "newly granted getAll");
      ownFunction(first.setEnabled, "newly granted setEnabled");
      assert(
        first.listContainsSelf && Number(first.extensionCount) > 0,
        "actual read-only management.getAll includes caller returned by getSelf",
      );
    }
    assertEquals(
      await command<boolean>("remove"),
      true,
      "real permissions.remove revokes management",
    );
    const removed = await until(async () => {
      const state = await command<MemberState>("state");
      return !state.contains && state.events.some((event) =>
        event.event === "removed" && event.permissions.includes("management")
      ) && state;
    }, "real management onRemoved in same Worker");
    assertEquals(
      removed.generation,
      initial.generation,
      "revoke preserves Worker generation",
    );
    const revoked = await command<MemberRevocation>("revoked");
    assertEquals(
      revoked.generation,
      initial.generation,
      "captured method checked in same revoked Worker",
    );
    assert(
      revoked.sameChrome && revoked.sameManagement && revoked.sameGetAll &&
        revoked.selfId === first.selfId,
      "SDK object and member references stay stable while public getSelf still works",
    );
    if (mode === "descriptor") {
      absent(revoked.getAll, "descriptor-frozen private member after revoke");
    } else {
      ownFunction(revoked.getAll, "retained getAll after revoke");
      assert(
        revoked.denial?.synchronous && !revoked.denial.succeeded &&
          revoked.denial.name === "Error" &&
          /management\.getAll.*not available in this context/.test(
            revoked.denial.message || "",
          ),
        "captured getAll synchronously denies after live revoke, matching genuine Chrome155 oracle",
      );
    }
    console.log(
      "[WorkerMemberPermissionsNative]",
      JSON.stringify({
        kind,
        mode,
        headed: true,
        action,
        initial,
        beforeConsent,
        granted,
        first,
        removed,
        revoked,
      }),
    );
  } finally {
    if (target) window.gBrowser.removeTab(target, { skipPermitUnload: true });
    if (control) window.gBrowser.removeTab(control, { skipPermitUnload: true });
    await addon?.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}
export async function runAllTests(): Promise<void> {
  await runTests(
    "WorkerMemberPermissionsNative",
    (["unread", "enumeration", "descriptor"] as const).flatMap((mode) =>
      (["classic", "module"] as const).map((kind) => ({
        name: `fresh ${kind} Worker management ${mode} grant and live revoke`,
        fn: () => fixture(kind, mode),
      }))
    ),
  );
}
