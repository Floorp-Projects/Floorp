// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Requires the compiled opt-in implementation and genuine headed Add-ons UI.
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type { ComponentAddonManager } from "./helpers/capture-component-test-types.ts";
import {
  inspectUserScriptsConsentUI,
  queryUserScriptsDocument,
  readUserScriptsConsentStatus,
  setUserScriptsConsentUsingUI,
} from "./helpers/user-scripts-consent-ui.mts";
import type {
  UserScriptsBarrierStatus,
  UserScriptsConsentCommandReply,
  UserScriptsConsentParent,
  UserScriptsConsentProbe,
  UserScriptsHeldDocumentSnapshot,
  UserScriptsInjectionSnapshot,
  UserScriptsUIBrowser,
  UserScriptsUITab,
  UserScriptsUIWindow,
} from "./helpers/user-scripts-consent-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function until<Value>(
  read: () =>
    | Value
    | false
    | null
    | undefined
    | Promise<Value | false | null | undefined>,
  description: string,
): Promise<Value> {
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw Error("Timed out waiting for " + description);
}

const backgroundSource = `
const generation=Math.random().toString(36)+':'+Date.now();
let captured,method;
async function probe(){
 const all=await browser.permissions.getAll();let internalRequestRejected=false;
 try{await browser.permissions.contains({permissions:['internal:floorpChromeUserScriptsAllowed']});}
 catch(error){internalRequestRejected=/permission|Invalid|value/i.test(error.message);}
 return {generation,namespace:typeof browser.userScripts,publicPermission:await browser.permissions.contains({permissions:['userScripts']}),privatePermissionsHidden:all.permissions.every(permission=>!permission.startsWith('internal:')),internalRequestRejected};
}
browser.runtime.onMessage.addListener(message=>{
 if(message.target!=='user-scripts-consent')return;
 return(async()=>{
  if(message.command==='probe')return probe();
  if(message.command==='register'){
   captured=browser.userScripts;method=captured?.getScripts;
   if(!captured||typeof method!=='function')throw Error('Allowed native userScripts API unavailable');
   await captured.register(['USER_SCRIPT','MAIN'].map(world=>({id:world==='MAIN'?'consent-retained-main':'consent-retained',world,matches:['http://127.0.0.1/*'],runAt:'document_end',js:[{code:'document.documentElement.setAttribute("data-native-user-scripts-consent-'+(world==='MAIN'?'main':'user')+'",'+JSON.stringify(message.token)+');'}]})));
   await captured.register([{id:'consent-start-sentinel',world:'USER_SCRIPT',matches:['http://127.0.0.1/*'],runAt:'document_start',js:[{code:'document.documentElement.setAttribute("data-native-user-scripts-consent-start",'+JSON.stringify(message.token)+');'}]}]);
   return {...await probe(),hasCaptured:true,scripts:await captured.getScripts(),sameNamespace:browser.userScripts===captured,sameMethod:browser.userScripts.getScripts===method};
  }
  if(message.command==='off'){
   if(!captured||!method)throw Error('Captured native API reference was lost');
   const current=browser.userScripts;let currentRejected=false,capturedRejected=false;
   try{await current.getScripts();}catch(error){currentRejected=/user.?scripts|permission|context|not available|disabled/i.test(error.message);}
   try{await Reflect.apply(method,captured,[]);}catch(error){capturedRejected=/user.?scripts|permission|context|not available|disabled/i.test(error.message);}
   return {...await probe(),hasCaptured:true,sameNamespace:current===captured,sameMethod:current?.getScripts===method,currentRejected,capturedRejected};
  }
  if(message.command==='retained')return {...await probe(),hasCaptured:!!captured,sameNamespace:browser.userScripts===captured,sameMethod:browser.userScripts?.getScripts===method,scripts:await Reflect.apply(method,captured,[])};
  throw Error('Unknown consent fixture command');
 })();
});
`;

async function userScriptsConsent(
  mode: "required" | "optional" | "ordinary",
): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "native Worker SDK is compiled",
  );
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: ComponentAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: UserScriptsConsentParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as UserScriptsUIWindow;
  const fixtureURL = window.wrappedJSObject.__floorpRuntimeFixtureURL;
  assert(typeof fixtureURL === "string", "controlled HTTP source is available");
  const converted = mode !== "ordinary";
  const original = mode === "required"
    ? "aaaaaaaaaaaabbbbbbbbbbbbcccccccc"
    : "ddddddddddddeeeeeeeeeeeeffffffff";
  const id = converted
    ? original + "@chromium-extension"
    : "ordinary-user-scripts-consent@example.test";
  const manifest = {
    manifest_version: 3,
    name: "Native user scripts " + mode + " consent fixture",
    version: "1.0",
    permissions: mode === "required" ? ["userScripts"] : [],
    optional_permissions: mode === "required" ? [] : ["userScripts"],
    host_permissions: ["http://127.0.0.1/*"],
    background: converted
      ? { service_worker: "background.js" }
      : { scripts: ["background.js"] },
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
  const controlSource = `
const generation='page:'+Math.random().toString(36)+':'+Date.now();let captured,method;
async function probe(){
 const all=await browser.permissions.getAll();let internalRequestRejected=false;
 try{await browser.permissions.contains({permissions:['internal:floorpChromeUserScriptsAllowed']});}
 catch(error){internalRequestRejected=/permission|Invalid|value/i.test(error.message);}
 return {generation,namespace:typeof browser.userScripts,publicPermission:await browser.permissions.contains({permissions:['userScripts']}),privatePermissionsHidden:all.permissions.every(permission=>!permission.startsWith('internal:')),internalRequestRejected};
}
async function pageCommand(message){
 if(message.command==='probe')return probe();
 if(message.command==='capture'){
  captured=browser.userScripts;method=captured?.getScripts;
  if(!captured||typeof method!=='function')throw Error('Allowed document userScripts API unavailable');
  return {...await probe(),hasCaptured:true,scripts:await captured.getScripts(),sameNamespace:browser.userScripts===captured,sameMethod:browser.userScripts.getScripts===method};
 }
 if(message.command==='off'){
  if(!captured||!method)throw Error('Captured document API reference was lost');
  const current=browser.userScripts;let currentRejected=false,capturedRejected=false;
  try{await current.getScripts();}catch(error){currentRejected=/user.?scripts|permission|context|not available|disabled/i.test(error.message);}
  try{await Reflect.apply(method,captured,[]);}catch(error){capturedRejected=/user.?scripts|permission|context|not available|disabled/i.test(error.message);}
  return {...await probe(),hasCaptured:true,sameNamespace:current===captured,sameMethod:current?.getScripts===method,currentRejected,capturedRejected};
 }
 if(message.command==='retained')return {...await probe(),hasCaptured:!!captured,sameNamespace:browser.userScripts===captured,sameMethod:browser.userScripts?.getScripts===method,scripts:await Reflect.apply(method,captured,[])};
 throw Error('Unknown document consent fixture command');
}
window.addEventListener('message',async event=>{
 if(!['user-scripts-consent','user-scripts-consent-page'].includes(event.data?.target))return;
 const {requestId}=event.data;
 try{document.title=JSON.stringify({requestId,result:event.data.target==='user-scripts-consent-page'?await pageCommand(event.data):await browser.runtime.sendMessage(event.data)});}
 catch(error){document.title=JSON.stringify({requestId,error:String(error)});}
});document.title='user-scripts-consent-ready';
`;
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "background.js": backgroundSource,
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js": controlSource,
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    "native-user-scripts-consent-" + Services.uuid.generateUUID() + ".xpi",
  );
  await IOUtils.write(
    path,
    zipSync(Object.fromEntries(
      Object.entries(files).map((
        [name, source],
      ) => [name, new TextEncoder().encode(source)]),
    )),
  );
  const tabs: UserScriptsUITab[] = [];
  const evidence: unknown[] = [];
  let releaseHeld: (() => Promise<void>) | null = null;
  const addon = await AddonManager.installTemporaryAddon(
    new FileUtils.File(path),
  );
  try {
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "installed consent fixture",
    );
    const background = await until(
      () => extension.backgroundContext,
      "native background started",
    );
    if (converted) {
      assertEquals(
        background.viewType,
        "background_worker",
        "converted fixture uses a real native Worker",
      );
    }
    const control = window.gBrowser.addTab(
      extension.baseURI.resolve("control.html"),
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    tabs.push(control);
    window.gBrowser.selectedTab = control;
    await until(
      () => control.linkedBrowser.contentTitle === "user-scripts-consent-ready",
      "real control page listener",
    );
    const controlInner = control.linkedBrowser.browsingContext
      .currentWindowGlobal?.innerWindowId;
    assert(
      typeof controlInner === "number",
      "the control page has an actual native document identity",
    );
    const command = async (
      name: string,
      data: Record<string, unknown> = {},
      browser: UserScriptsUIBrowser = control.linkedBrowser,
    ): Promise<UserScriptsConsentProbe> => {
      const requestId = String(Services.uuid.generateUUID());
      await queryUserScriptsDocument(
        browser,
        `()=>{content.postMessage(${
          JSON.stringify({
            target: "user-scripts-consent",
            command: name,
            requestId,
            ...data,
          })
        },'*');return true;}`,
      );
      return await until(() => {
        const title = browser.contentTitle;
        if (!title.startsWith("{")) return false;
        const reply = JSON.parse(title) as UserScriptsConsentCommandReply;
        if (reply.requestId !== requestId) return false;
        if (reply.error) throw Error(reply.error);
        assert(reply.result, "actual native context returned consent state");
        return reply.result;
      }, "correlated native consent command " + name);
    };
    const pageCommand = (
      name: string,
      browser: UserScriptsUIBrowser = control.linkedBrowser,
    ) => command(name, { target: "user-scripts-consent-page" }, browser);
    const initial = await command("probe");
    const initialPage = await pageCommand("probe");
    evidence.push({
      phase: "initial",
      ...initial,
      status: await readUserScriptsConsentStatus(id),
      page: initialPage,
    });
    assertEquals(
      initial.namespace,
      "undefined",
      "fresh OFF context has no userScripts namespace",
    );
    assertEquals(
      initial.publicPermission,
      mode === "required",
      "public permission follows only the manifest grant",
    );
    assertEquals(
      initialPage.namespace,
      "undefined",
      "fresh OFF extension document has no userScripts namespace",
    );
    assertEquals(
      initialPage.publicPermission,
      mode === "required",
      "document public permission follows only its manifest grant",
    );
    assert(
      initialPage.privatePermissionsHidden &&
        initialPage.internalRequestRejected,
      "document permissions API never exposes the private consent flag",
    );
    assert(
      initial.privatePermissionsHidden && initial.internalRequestRejected,
      "private consent flag cannot be queried through public permissions",
    );
    if (!converted) {
      const ui = await inspectUserScriptsConsentUI(id, window);
      const status = await readUserScriptsConsentStatus(id);
      evidence.push({ phase: "ordinary", ui, status });
      assert(
        !ui.toggleVisible,
        "ordinary Firefox addons have no Chrome consent toggle",
      );
      assert(
        !status.eligible && !status.allowed,
        "ordinary Firefox addon cannot opt in",
      );
      return;
    }
    const allowed = await setUserScriptsConsentUsingUI(id, true, window);
    evidence.push(allowed);
    assertEquals(
      allowed.statusAfter.permissionGranted,
      mode === "required",
      "Allow UI never grants the public userScripts permission",
    );
    if (mode === "required") {
      // Genuine Chrome leaves an unread namespace hidden after ON -> OFF;
      // granting consent must not eagerly materialize a native SDK object.
      evidence.push(await setUserScriptsConsentUsingUI(id, false, window));
      const unread = await command("probe");
      const unreadPage = await pageCommand("probe");
      evidence.push({
        phase: "off-without-on-namespace-read",
        ...unread,
        page: unreadPage,
      });
      assertEquals(
        unread.namespace,
        "undefined",
        "an unread ON namespace stays hidden after OFF",
      );
      assertEquals(
        unread.generation,
        initial.generation,
        "unread consent changes keep the same Worker",
      );
      assertEquals(
        unreadPage.namespace,
        "undefined",
        "unread ON document namespace remains hidden after OFF",
      );
      assertEquals(
        unreadPage.generation,
        initialPage.generation,
        "unread consent changes preserve the actual extension document",
      );
      evidence.push(await setUserScriptsConsentUsingUI(id, true, window));
    }
    const on = await command("probe");
    assertEquals(
      on.generation,
      initial.generation,
      "UI Allow preserves the same native Worker",
    );
    assert(
      on.privatePermissionsHidden && on.internalRequestRejected,
      "saved consent stays private after Allow",
    );
    if (mode === "optional") {
      const optionalPage = await pageCommand("probe");
      evidence.push({ phase: "optional-page-on", ...optionalPage });
      assertEquals(
        on.namespace,
        "undefined",
        "private Allow alone does not expose an ungranted optional API",
      );
      assertEquals(
        on.publicPermission,
        false,
        "optional public permission remains ungranted",
      );
      assertEquals(
        optionalPage.namespace,
        "undefined",
        "private Allow alone does not expose an optional document API",
      );
      assertEquals(
        optionalPage.publicPermission,
        false,
        "Allow does not grant the document's optional public permission",
      );
      evidence.push(await setUserScriptsConsentUsingUI(id, false, window));
      return;
    }
    const token = String(Services.uuid.generateUUID());
    const registered = await command("register", { token });
    const capturedPage = await pageCommand("capture");
    evidence.push({ phase: "registered", ...registered, page: capturedPage });
    assertEquals(
      registered.generation,
      initial.generation,
      "same Worker registers the actual user script",
    );
    assertEquals(
      capturedPage.generation,
      initialPage.generation,
      "the same actual document captures its native API",
    );
    assert(
      capturedPage.sameNamespace && capturedPage.sameMethod,
      "the native document API retains its captured references",
    );
    assertEquals(
      JSON.stringify(capturedPage.scripts?.map((script) => script.id).sort()),
      JSON.stringify(registered.scripts?.map((script) => script.id).sort()),
      "document and Worker read the same real registrations",
    );
    assertEquals(
      JSON.stringify(registered.scripts?.map((script) => script.id).sort()),
      JSON.stringify([
        "consent-retained",
        "consent-retained-main",
        "consent-start-sentinel",
      ]),
      "real USER_SCRIPT and MAIN registrations are readable",
    );
    const documentIds = new Set<number>();
    const checkLoadedDocument = async (
      tab: UserScriptsUITab,
      injected: boolean,
      startInjected = injected,
    ) => {
      await until(
        () =>
          queryUserScriptsDocument<boolean>(
            tab.linkedBrowser,
            `()=>content.document.readyState==='complete'`,
          ),
        "new document completed loading",
      );
      if (!injected) await new Promise((resolve) => setTimeout(resolve, 200));
      const observed = await until(async () => {
        const value = await queryUserScriptsDocument<
          UserScriptsInjectionSnapshot
        >(
          tab.linkedBrowser,
          `()=>({user:content.document.documentElement.getAttribute('data-native-user-scripts-consent-user'),main:content.document.documentElement.getAttribute('data-native-user-scripts-consent-main'),start:content.document.documentElement.getAttribute('data-native-user-scripts-consent-start')})`,
        );
        return !injected ||
            value.user === token && value.main === token &&
              value.start === token
          ? value
          : false;
      }, "actual policy-gated new-document injection");
      assertEquals(
        JSON.stringify([observed.user, observed.main, observed.start]),
        JSON.stringify([
          injected ? token : null,
          injected ? token : null,
          startInjected ? token : null,
        ]),
        "only allowed USER_SCRIPT and MAIN registrations inject into fresh actual documents",
      );
      const inner = tab.linkedBrowser.browsingContext.currentWindowGlobal
        ?.innerWindowId;
      assert(
        typeof inner === "number" && !documentIds.has(inner),
        "injection proof uses a distinct real native document",
      );
      documentIds.add(inner);
      evidence.push({
        phase: injected ? "injected" : "blocked-injection",
        innerWindowId: inner,
        ...observed,
      });
    };
    const checkNewDocument = async (injected: boolean) => {
      const tab = window.gBrowser.addTab(
        fixtureURL + "worker-target.html?consent=" +
          Services.uuid.generateUUID(),
        {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        },
      );
      tabs.push(tab);
      await until(
        () => tab.linkedBrowser.contentTitle === "Worker injection target",
        "distinct controlled injection target",
      );
      await checkLoadedDocument(tab, injected);
    };
    await checkNewDocument(true);

    // A real synchronous external script holds this parser before document_end.
    // The start sentinel proves matching/execution while consent was still ON.
    const barrierToken = String(Services.uuid.generateUUID()).toLowerCase()
      .replace(/[^a-f0-9]/g, "");
    assert(
      /^[a-f0-9]{32}$/.test(barrierToken),
      "owned parser barrier has an exact random token",
    );
    const barrierURL = fixtureURL + "user-scripts-consent/";
    const barrierStatus = () =>
      queryUserScriptsDocument<UserScriptsBarrierStatus>(
        control.linkedBrowser,
        `async()=>{const response=await content.fetch(${
          JSON.stringify(barrierURL + "status?token=" + barrierToken)
        });if(!response.ok)throw Error('Barrier status HTTP '+response.status);return await response.json();}`,
      );
    releaseHeld = async () => {
      await queryUserScriptsDocument(
        control.linkedBrowser,
        `async()=>{const response=await content.fetch(${
          JSON.stringify(barrierURL + "release?token=" + barrierToken)
        },{method:'POST'});if(!response.ok)throw Error('Barrier release HTTP '+response.status);return true;}`,
      );
    };
    const held = window.gBrowser.addTab(
      barrierURL + "held.html?token=" + barrierToken,
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    tabs.push(held);
    const heldBefore = await until(async () => {
      const state = await barrierStatus();
      if (state.timedOut) {
        throw Error(
          "Actual parser barrier timed out before consent revocation",
        );
      }
      if (!state.scriptRequested) return false;
      const document = await queryUserScriptsDocument<
        UserScriptsHeldDocumentSnapshot
      >(
        held.linkedBrowser,
        `()=>({readyState:content.document.readyState,start:content.document.documentElement.getAttribute('data-native-user-scripts-consent-start'),user:content.document.documentElement.getAttribute('data-native-user-scripts-consent-user'),main:content.document.documentElement.getAttribute('data-native-user-scripts-consent-main')})`,
      );
      return document.readyState === "loading" && document.start === token &&
        { state, document };
    }, "real ON document_start sentinel and externally held parser");
    assertEquals(
      JSON.stringify([heldBefore.document.user, heldBefore.document.main]),
      JSON.stringify([null, null]),
      "both document_end worlds remain pending behind the real parser barrier",
    );
    const heldInner = held.linkedBrowser.browsingContext.currentWindowGlobal
      ?.innerWindowId;
    assert(
      typeof heldInner === "number",
      "held parser belongs to an actual native document",
    );
    evidence.push({
      phase: "matched-while-on-before-document-end",
      innerWindowId: heldInner,
      ...heldBefore,
    });
    evidence.push(await setUserScriptsConsentUsingUI(id, false, window));
    const off = await command("off");
    const offPage = await pageCommand("off");
    evidence.push({ phase: "off", ...off, page: offPage });
    assertEquals(
      offPage.generation,
      initialPage.generation,
      "OFF preserves the same actual extension document",
    );
    assert(
      offPage.sameNamespace && offPage.sameMethod,
      "OFF retains the same previously read document namespace and method",
    );
    assert(
      offPage.currentRejected && offPage.capturedRejected,
      "OFF denies current and captured document API references",
    );
    assert(
      offPage.publicPermission && offPage.privatePermissionsHidden &&
        offPage.internalRequestRejected,
      "OFF retains only the real document public grant and hides its private flag",
    );
    assertEquals(
      off.generation,
      initial.generation,
      "OFF preserves the same native Worker",
    );
    assert(
      off.sameNamespace && off.sameMethod,
      "previously exposed native namespace and method retain identity",
    );
    assert(
      off.currentRejected && off.capturedRejected,
      "OFF denies both current and captured backend references",
    );
    assertEquals(
      off.publicPermission,
      true,
      "required public permission remains granted while Allow is OFF",
    );
    assert(
      off.privatePermissionsHidden && off.internalRequestRejected,
      "OFF does not expose internal consent permissions",
    );
    await releaseHeld();
    releaseHeld = null;
    const released = await barrierStatus();
    assert(
      released.scriptRequested && released.released && !released.timedOut,
      "only the owned explicit release completed the actual parser barrier",
    );
    await checkLoadedDocument(held, false, true);
    assertEquals(
      held.linkedBrowser.browsingContext.currentWindowGlobal?.innerWindowId,
      heldInner,
      "revocation proof completes the same previously matched native document",
    );
    await checkNewDocument(false);
    const freshOffControl = window.gBrowser.addTab(
      extension.baseURI.resolve("control.html"),
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    tabs.push(freshOffControl);
    await until(
      () =>
        freshOffControl.linkedBrowser.contentTitle ===
          "user-scripts-consent-ready",
      "new actual OFF extension document",
    );
    const freshOffPage = await pageCommand(
      "probe",
      freshOffControl.linkedBrowser,
    );
    const freshOffInner = freshOffControl.linkedBrowser.browsingContext
      .currentWindowGlobal?.innerWindowId;
    assert(
      typeof freshOffInner === "number" && freshOffInner !== controlInner,
      "fresh OFF namespace proof uses a distinct native extension document",
    );
    assert(
      freshOffPage.generation !== initialPage.generation,
      "fresh OFF extension page has its own actual JavaScript realm",
    );
    assertEquals(
      freshOffPage.namespace,
      "undefined",
      "a fresh OFF extension document hides the native namespace",
    );
    assert(
      freshOffPage.publicPermission && freshOffPage.privatePermissionsHidden &&
        freshOffPage.internalRequestRejected,
      "the fresh OFF document preserves public grant and private flag isolation",
    );
    evidence.push({
      phase: "fresh-off-extension-page",
      innerWindowId: freshOffInner,
      ...freshOffPage,
    });
    evidence.push(await setUserScriptsConsentUsingUI(id, true, window));
    const resumed = await command("retained");
    const resumedPage = await pageCommand("retained");
    evidence.push({ phase: "allowed-again", ...resumed, page: resumedPage });
    assertEquals(
      resumedPage.generation,
      initialPage.generation,
      "ON again keeps the original actual extension page realm",
    );
    assertEquals(
      control.linkedBrowser.browsingContext.currentWindowGlobal?.innerWindowId,
      controlInner,
      "ON/OFF/ON preserved the exact native document",
    );
    assert(
      resumedPage.sameNamespace && resumedPage.sameMethod,
      "ON reuses the original document API references",
    );
    assertEquals(
      JSON.stringify(resumedPage.scripts?.map((script) => script.id).sort()),
      JSON.stringify(resumed.scripts?.map((script) => script.id).sort()),
      "the same document API resumes with exactly the retained Worker registrations",
    );
    assertEquals(
      resumed.generation,
      initial.generation,
      "ON again preserves the original Worker",
    );
    assert(
      resumed.sameNamespace && resumed.sameMethod,
      "ON reuses the original captured API references",
    );
    assertEquals(
      JSON.stringify(resumed.scripts?.map((script) => script.id).sort()),
      JSON.stringify([
        "consent-retained",
        "consent-retained-main",
        "consent-start-sentinel",
      ]),
      "OFF suspended injection without deleting the registration",
    );
    await checkNewDocument(true);
  } finally {
    try {
      await releaseHeld?.();
    } finally {
      try {
        await addon.uninstall();
      } finally {
        for (const tab of tabs) {
          if (tab.isConnected) window.gBrowser.removeTab(tab);
        }
        await IOUtils.remove(path, { ignoreAbsent: true });
        console.log(
          "[ChromeUserScriptsConsentNative]",
          JSON.stringify({ mode, id, evidence }),
        );
      }
    }
  }
}

export function runAllTests(): Promise<void> {
  return runTests("ChromeUserScriptsConsentNative.test.mts", [
    {
      name:
        "real Allow UI gates same-Worker and same-page references and retained injections",
      fn: () => userScriptsConsent("required"),
    },
    {
      name: "Allow does not grant an optional public permission",
      fn: () => userScriptsConsent("optional"),
    },
    {
      name: "ordinary Firefox addon keeps its original consent boundary",
      fn: () => userScriptsConsent("ordinary"),
    },
  ]);
}
