// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function waitFor<T>(
  read: () => T | undefined | false,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

const workerBody = `
  let offscreenMessages=0;
  browser.omnibox.onInputChanged.addListener((text,suggest)=>{
    suggest([{content:text+"-first",description:"First native Worker suggestion"}]);
    suggest([{content:text+"-second",description:"Second native Worker suggestion"}]);
    setTimeout(()=>suggest([{content:text+"-async",description:"Asynchronous native Worker suggestion"}]),20);
  });
  const ready=(async()=>{
    const {starts=0}=await browser.storage.local.get("starts");
    await browser.storage.local.set({starts:starts+1});
    return starts+1;
  })();
  browser.runtime.onMessage.addListener((message,sender,sendResponse)=>{
    if(message.workerTarget!==true) return;
    if(message.command==="callback-message") {
      Promise.resolve().then(()=>sendResponse({callback:true}));
      return true;
    }
    return (async()=>{
      const starts=await ready;
      switch(message.command) {
        case "probe": return {
          imported,starts,isWorker:self instanceof ServiceWorkerGlobalScope,
          hasDocument:typeof document!=="undefined",sameAlias:browser===chrome,
          contextTypes:(await browser.runtime.getContexts({})).map(c=>c.contextType),
        };
        case "storage": {
          await browser.storage.local.set({value:{nested:[1,"two",true]}});
          const local=await browser.storage.local.get("value");
          await browser.storage.session.set({sessionValue:"retained"});
          const session=await browser.storage.session.get("sessionValue");
          const callback=await new Promise(resolve=>chrome.storage.local.get("value",resolve));
          await browser.storage.local.remove("value");
          return {local,session,callback,removed:await browser.storage.local.get("value")};
        }
        case "apis": {
          const tabs=await browser.tabs.query({});
          await browser.action.setTitle({title:"Worker native action"});
          const title=await browser.action.getTitle({});
          await browser.declarativeNetRequest.updateDynamicRules({removeRuleIds:[91],addRules:[{
            id:91,priority:1,action:{type:"block"},condition:{urlFilter:"worker-blocked.invalid",resourceTypes:["main_frame"]},
          }]});
          const rules=await browser.declarativeNetRequest.getDynamicRules();
          await browser.declarativeNetRequest.updateDynamicRules({removeRuleIds:[91]});
          await browser.sidePanel.setOptions({path:"panel.html",enabled:true});
          const sidePanel=await browser.sidePanel.getOptions({});
          await browser.offscreen.createDocument({url:"offscreen.html",reasons:["DOM_PARSER"],justification:"Worker native API integration"});
          const contexts=await browser.runtime.getContexts({contextTypes:["OFFSCREEN_DOCUMENT"]});
          const offscreen=await browser.runtime.sendMessage({offscreenTarget:true});
          await browser.offscreen.closeDocument();
          await browser.userScripts.register([{id:"worker-native",matches:["http://127.0.0.1/*"],js:[{code:'document.documentElement.setAttribute("data-worker-user-script","native");'}],runAt:"document_idle"}]);
          const scripts=await browser.userScripts.getScripts();
          return {tabs:tabs.length,title,rules,sidePanel,contexts,offscreen,scripts};
        }
        case "core": {
          const extensionInfo={
            incognito:browser.extension.inIncognitoContext,
            fileAllowed:await browser.extension.isAllowedFileSchemeAccess(),
            privateAllowed:await browser.extension.isAllowedIncognitoAccess(),
            viewType:browser.extension.ViewType,
            getViews:typeof browser.extension.getViews,
            getBackgroundPage:typeof browser.extension.getBackgroundPage,
            getURL:typeof browser.extension.getURL,
            updateDataUndefined:browser.extension.setUpdateUrlData("ignored-for-authenticated-CWS-install")===undefined,
          };
          let invalidUpdateCalls=0;
          for(const args of [[],[3],["x".repeat(1025)],["one","extra"]]) {
            try {Reflect.apply(browser.extension.setUpdateUrlData,browser.extension,args);}catch {invalidUpdateCalls++;}
          }
          const power={
            level:browser.power.Level,
            requestUndefined:browser.power.requestKeepAwake(browser.power.Level.SYSTEM)===undefined,
            releaseUndefined:browser.power.releaseKeepAwake()===undefined,
          };
          const translated=browser.i18n.getMessage("worker_message",["native"]);
          const language=browser.i18n.getUILanguage();
          const languages=await browser.i18n.getAcceptLanguages();
          browser.idle.setDetectionInterval(15);
          const idle=await browser.idle.queryState(15);
          browser.idle.setDetectionInterval(60);
          const bookmarkEvents=[];
          const onCreated=(id,node)=>bookmarkEvents.push({id,title:node.title});
          browser.bookmarks.onCreated.addListener(onCreated);
          let bookmark,readBookmark;
          try {
            bookmark=await browser.bookmarks.create({title:"Actual Worker bookmark",url:message.url});
            readBookmark=await browser.bookmarks.get(bookmark.id);
          } finally {
            if(bookmark)await browser.bookmarks.remove(bookmark.id);
            browser.bookmarks.onCreated.removeListener(onCreated);
          }
          await browser.history.addUrl({url:message.url,title:"Actual Worker history"});
          const visits=await browser.history.getVisits({url:message.url});
          await browser.history.deleteUrl({url:message.url});
          const closed=await browser.sessions.getRecentlyClosed({maxResults:3});
          const topSites=await browser.topSites.get();
          const commands=await browser.commands.getAll();
          const windows=await browser.windows.getAll();
          const current=await browser.windows.getCurrent();
          return {extensionInfo,invalidUpdateCalls,power,translated,language,languages,idle,bookmark,readBookmark,bookmarkEvents,visits,closed,topSites,commands,windows,current};
        }
        case "additional": {
          const permissions=await browser.permissions.getAll();
          const self=await browser.management.getSelf();
          const redirect=browser.identity.getRedirectURL("native-worker");
          const cookie=await browser.cookies.set({url:message.url,name:"native-worker",value:"real"});
          const readCookie=await browser.cookies.get({url:message.url,name:"native-worker"});
          await browser.cookies.remove({url:message.url,name:"native-worker"});
          const privacy=await browser.privacy.network.networkPredictionEnabled.get({});
          const notifications=await browser.notifications.getAll();
          const downloads=await browser.downloads.search({limit:1});
          const frames=await browser.webNavigation.getAllFrames({tabId:message.tabId});
          let menuId;
          const menuCallback=await new Promise((resolve,reject)=>{
            menuId=chrome.contextMenus.create({id:"worker-menu",title:"Worker menu",contexts:["all"]},function(){
              const error=chrome.runtime.lastError;
              if(error)reject(new Error(error.message));else resolve(arguments.length);
            });
          });
          const duplicateMenuError=await new Promise(resolve=>chrome.contextMenus.create({id:"worker-menu",title:"duplicate"},()=>resolve(chrome.runtime.lastError?.message)));
          await browser.contextMenus.remove("worker-menu");
          const groupId=await browser.tabs.group({tabIds:[message.tabId]});
          await browser.tabGroups.update(groupId,{title:"Worker native group"});
          const group=await browser.tabGroups.get(groupId);
          await browser.tabs.ungroup([message.tabId]);
          return {permissions,self,redirect,cookie,readCookie,privacy,notifications,downloads,frames,menuId,menuCallback,duplicateMenuError,group};
        }
        case "auxiliary": {
          await browser.storage.local.setAccessLevel({accessLevel:"TRUSTED_CONTEXTS"});
          const notificationPermission=await browser.notifications.getPermissionLevel();
          const fonts=await browser.fontSettings.getFontList();
          const before=await browser.fontSettings.getDefaultFontSize({});
          let fontChange;
          const onFont=details=>{fontChange=details;};
          browser.fontSettings.onDefaultFontSizeChanged.addListener(onFont);
          try {
            await browser.fontSettings.setDefaultFontSize({pixelSize:before.pixelSize+1});
            const changed=await browser.fontSettings.getDefaultFontSize({});
            const voices=await browser.tts.getVoices();
            if(!voices.length) throw new Error("Native TTS integration requires an actual installed voice");
            const voice=voices.find(v=>v.lang?.startsWith("en"))||voices[0];
            const events=[];
            let resolveFinal,rejectFinal;
            const final=new Promise((resolve,reject)=>{resolveFinal=resolve;rejectFinal=reject;});
            const timeout=setTimeout(()=>rejectFinal(new Error("Native Worker TTS final event timed out")),15000);
            try {
              await browser.tts.speak("Native worker speech.",{voiceName:voice.voiceName,onEvent:event=>{
                events.push(event);
                if(event.type==="end")resolveFinal();
                if(["error","interrupted","cancelled"].includes(event.type))rejectFinal(new Error(JSON.stringify(event)));
              }});
              await final;
            } finally {clearTimeout(timeout);}
            return {notificationPermission,fontCount:fonts.length,before,changed,fontChange,voiceCount:voices.length,events};
          } finally {
            browser.fontSettings.onDefaultFontSizeChanged.removeListener(onFont);
            await browser.fontSettings.setDefaultFontSize({pixelSize:before.pixelSize});
          }
        }
        case "declarative": {
          const api=browser.declarativeContent;
          const matcher=new api.PageStateMatcher({pageUrl:{urlContains:"worker-target"},css:["body"]});
          const show=new api.ShowAction();
          const legacyShow=new api.ShowPageAction();
          const script=new api.RequestContentScript({js:["declarative.js"]});
          const icon=new api.SetIcon({imageData:new ImageData(new Uint8ClampedArray([255,0,0,255]),1,1)});
          let requiresNew=false;
          try {api.PageStateMatcher({});} catch {requiresNew=true;}
          await api.onPageChanged.addRules([{id:"worker-native-declarative",conditions:[matcher],actions:[show,script,icon]}]);
          const rules=await api.onPageChanged.getRules(["worker-native-declarative"]);
          return {requiresNew,matcherInstance:matcher instanceof api.PageStateMatcher,showInstance:show instanceof api.ShowAction,iconPlain:!(icon instanceof api.SetIcon),legacyType:legacyShow.instanceType,rules};
        }
        case "scripting": {
          const target={tabId:message.tabId};
          const result=await browser.scripting.executeScript({target,func:(left,right)=>({sum:left+right,title:document.title,userScript:document.documentElement.getAttribute("data-worker-user-script"),declarative:document.documentElement.getAttribute("data-worker-declarative")}),args:[2,3]});
          const file=await browser.scripting.executeScript({target,files:["injected.js"]});
          const callbackError=await new Promise(resolve=>chrome.scripting.executeScript({target:{tabId:2147483000},func:()=>1},()=>resolve(chrome.runtime.lastError?.message)));
          const cyclic={};cyclic.self=cyclic;
          let cyclicRejected=false;
          try {await browser.scripting.executeScript({target,func:()=>1,args:[cyclic]});}catch {cyclicRejected=true;}
          await browser.userScripts.unregister({ids:["worker-native"]});
          await browser.declarativeContent.onPageChanged.removeRules(["worker-native-declarative"]);
          return {result,file,callbackError,cyclicRejected};
        }
        case "session": return browser.storage.session.get("sessionValue");
      }
      throw new Error("Unknown worker command");
    })();
  });
`;

async function testWorker(type: "classic" | "module"): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "native Worker bindings are compiled",
  );
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  const fixtureURL = win.wrappedJSObject.__floorpRuntimeFixtureURL;
  assert(typeof fixtureURL === "string", "runner provides local HTTP fixtures");
  const originalId = type === "classic"
    ? "aaaaaaaabbbbbbbbccccccccdddddddd"
    : "eeeeeeeeffffffffgggggggghhhhhhhh";
  const id = `${originalId}@chromium-extension`;
  const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  const target = win.gBrowser.addTab(fixtureURL + "worker-target.html", {
    triggeringPrincipal: systemPrincipal,
  });
  await waitFor(
    () => target.linkedBrowser.contentTitle === "Worker injection target",
    "HTTP injection target",
  );
  const manifest = {
    manifest_version: 3,
    name: `Native ${type} Worker regression fixture`,
    version: "1.0",
    permissions: [
      "storage",
      "tabs",
      "scripting",
      "offscreen",
      "sidePanel",
      "declarativeNetRequest",
      "userScripts",
      "identity",
      "management",
      "cookies",
      "privacy",
      "notifications",
      "downloads",
      "webNavigation",
      "contextMenus",
      "tabGroups",
      "fontSettings",
      "tts",
      "declarativeContent",
      "bookmarks",
      "history",
      "idle",
      "sessions",
      "topSites",
      "power",
    ],
    host_permissions: ["http://127.0.0.1/*"],
    action: {},
    default_locale: "en",
    commands: {
      "worker-command": { description: "Actual native Worker command" },
    },
    omnibox: { keyword: `floorp-${type}-worker` },
    background: {
      service_worker: "worker.js",
      ...(type === "module" ? { type: "module" } : {}),
    },
    browser_specific_settings: {
      gecko: {
        id,
        floorp_chrome_extension: { schema_version: 1, original_id: originalId },
      },
    },
  };
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "worker.js":
      (type === "classic"
        ? 'importScripts("helper.js");const imported=self.imported;'
        : 'import { imported } from "./helper.mjs";') + workerBody,
    "helper.js": 'self.imported="classic-importScripts";',
    "helper.mjs": 'export const imported="module-static-import";',
    "_locales/en/messages.json": JSON.stringify({
      worker_message: { message: "Hello $1" },
    }),
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js": `
      document.title="control-ready";
      window.addEventListener("message",async event=>{
        if(event.data.workerTarget!==true) return;
        try {
          let result;
          if(event.data.command==="container") {
            const ready=await navigator.serviceWorker.ready;
            const registration=await navigator.serviceWorker.getRegistration();
            result={native:navigator.serviceWorker instanceof ServiceWorkerContainer,scope:ready.scope,scriptURL:ready.active?.scriptURL,sameRegistration:registration===ready};
          } else result=await browser.runtime.sendMessage(event.data);
          document.title=JSON.stringify({requestId:event.data.requestId,result});
        }
        catch(error) {document.title=JSON.stringify({requestId:event.data.requestId,error:error.message});}
      });
    `,
    "offscreen.html": '<!doctype html><script src="offscreen.js"></script>',
    "offscreen.js":
      "browser.runtime.onMessage.addListener(message=>message.offscreenTarget?Promise.resolve({offscreen:true}):undefined);",
    "panel.html": "<!doctype html><title>Worker side panel</title>",
    "injected.js": '({file:"native-file",title:document.title});',
    "declarative.js":
      'document.documentElement.setAttribute("data-worker-declarative","native");',
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-${type}-worker-${Services.uuid.generateUUID()}.xpi`,
  );
  await IOUtils.write(
    path,
    zipSync(
      Object.fromEntries(
        Object.entries(files).map(([name, text]) => [
          name,
          new TextEncoder().encode(text),
        ]),
      ),
    ),
  );
  let addon: { uninstall(): Promise<void> } | undefined;
  let control: typeof target | undefined;
  let requestId = 0;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "Worker extension startup",
    );
    await waitFor(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual Worker context",
    );
    assert(
      extension.policy.backgroundServiceWorkerAllowed,
      "validated compatibility marker enables only this policy",
    );
    assertEquals(
      extension.policy.backgroundTypeModule,
      type === "module",
      "native Worker type follows manifest",
    );
    control = win.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: systemPrincipal,
    });
    await waitFor(
      () => control?.linkedBrowser.contentTitle === "control-ready",
      "message control document",
    );
    const command = async (
      name: string,
      payload: Record<string, unknown> = {},
    ) => {
      const current = ++requestId;
      control?.linkedBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            `content.postMessage(${
              JSON.stringify({
                workerTarget: true,
                command: name,
                requestId: current,
                ...payload,
              })
            },"*");`,
          ),
        false,
      );
      return await waitFor(() => {
        const title = control?.linkedBrowser.contentTitle;
        if (!title?.startsWith("{")) return false;
        const result = JSON.parse(title);
        if (result.requestId !== current) return false;
        if (result.error) throw new Error(result.error);
        return result;
      }, `Worker ${name} response`);
    };
    const container = (await command("container")).result;
    assert(
      container.native,
      "extension page exposes actual native ServiceWorkerContainer",
    );
    assertEquals(
      container.scope,
      extension.baseURI.spec,
      "page ready resolves native extension registration",
    );
    assertEquals(
      container.scriptURL,
      extension.baseURI.resolve("worker.js"),
      "page sees actual background worker script",
    );
    assert(
      container.sameRegistration,
      "getRegistration and ready return same native registration",
    );
    const { ExtensionSearchHandler } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionSearchHandler.sys.mjs",
    );
    const suggestions: { content: string }[][] = [];
    const search = ExtensionSearchHandler.handleSearch({
      keyword: `floorp-${type}-worker`,
      text: `floorp-${type}-worker query`,
      inPrivateWindow: false,
    }, (items: { content: string }[]) => suggestions.push(items));
    try {
      await waitFor(
        () => suggestions.length >= 3,
        "repeatable native omnibox callback suggestions",
      );
      assertEquals(
        suggestions[0][0].content,
        "query-first",
        "first suggestion reaches actual omnibox handler",
      );
      assertEquals(
        suggestions[1][0].content,
        "query-second",
        "same native suggestion callback remains callable",
      );
      assertEquals(
        suggestions[2][0].content,
        "query-async",
        "asynchronous suggestion callback reaches native search",
      );
    } finally {
      ExtensionSearchHandler.handleInputCancelled();
      await search;
    }
    const probe = (await command("probe")).result;
    assert(
      probe.isWorker && !probe.hasDocument && probe.sameAlias,
      "real ServiceWorker global and native chrome/browser aliases",
    );
    assertEquals(
      probe.imported,
      type === "classic" ? "classic-importScripts" : "module-static-import",
      "actual native script/module imports",
    );
    assert(
      probe.contextTypes.includes("BACKGROUND"),
      "runtime.getContexts includes Worker context",
    );
    const storage = (await command("storage")).result;
    assertEquals(
      storage.local.value.nested[1],
      "two",
      "storage.local persists nested values",
    );
    assertEquals(
      storage.callback.value.nested[0],
      1,
      "storage callback receives native result",
    );
    assertEquals(
      storage.session.sessionValue,
      "retained",
      "storage.session is real",
    );
    assertEquals(
      Object.keys(storage.removed).length,
      0,
      "storage.local.remove updates backend",
    );
    const core =
      (await command("core", { url: fixtureURL + "worker-core-history.html" }))
        .result;
    assertEquals(
      core.extensionInfo.incognito,
      false,
      "extension namespace returns actual regular Worker context state",
    );
    assert(
      typeof core.extensionInfo.fileAllowed === "boolean" &&
        typeof core.extensionInfo.privateAllowed === "boolean",
      "extension access checks use native permission state",
    );
    assertEquals(
      core.extensionInfo.viewType.TAB,
      "tab",
      "extension.ViewType matches measured Chrome enum",
    );
    assertEquals(
      core.extensionInfo.viewType.POPUP,
      "popup",
      "extension.ViewType popup constant",
    );
    assert(
      [
        core.extensionInfo.getViews,
        core.extensionInfo.getBackgroundPage,
        core.extensionInfo.getURL,
      ].every((value) => value === "undefined"),
      "DOM/MV2 extension aliases remain absent as in actual Chrome Worker",
    );
    assert(
      core.extensionInfo.updateDataUndefined && core.invalidUpdateCalls === 4,
      "Store-only update data has exact sync return and strict arguments",
    );
    assert(
      core.power.level.SYSTEM === "system" &&
        core.power.level.DISPLAY === "display",
      "power.Level matches actual Chrome enum",
    );
    assert(
      core.power.requestUndefined && core.power.releaseUndefined,
      "power native WakeLock APIs return synchronously",
    );
    assertEquals(
      core.translated,
      "Hello native",
      "Worker i18n synchronously uses actual catalog substitutions",
    );
    assert(
      typeof core.language === "string" && core.languages.length > 0,
      "Worker locale APIs return actual browser locale data",
    );
    assert(
      ["active", "idle", "locked"].includes(core.idle),
      "Worker idle queries native user idle service",
    );
    assertEquals(
      core.readBookmark[0].id,
      core.bookmark.id,
      "Worker bookmarks create/get uses actual Places record",
    );
    assert(
      core.bookmarkEvents.some((event: { id: string }) =>
        event.id === core.bookmark.id
      ),
      "native bookmark created event reaches Worker",
    );
    assert(
      core.visits.length > 0,
      "Worker history stores and reads real Places visit",
    );
    assert(
      Array.isArray(core.closed) && Array.isArray(core.topSites),
      "Worker sessions/topSites query native backends",
    );
    assert(
      core.commands.some((command: { name: string }) =>
        command.name === "worker-command"
      ),
      "Worker commands sees manifest command",
    );
    assert(
      core.windows.some((window: { id: number }) =>
        window.id === core.current.id
      ),
      "Worker windows returns actual current native window",
    );
    const apis = (await command("apis")).result;
    assert(apis.tabs > 0, "tabs.query returns actual browser tabs");
    assertEquals(
      apis.title,
      "Worker native action",
      "action setter/getter reaches native implementation",
    );
    assertEquals(
      apis.rules[0].id,
      91,
      "DNR dynamic rule persisted in native store",
    );
    assertEquals(
      apis.sidePanel.path,
      "panel.html",
      "sidePanel configuration is real",
    );
    assertEquals(
      apis.contexts.length,
      1,
      "Worker creates actual Offscreen document",
    );
    assert(
      apis.offscreen.offscreen,
      "Worker exchanges messages with actual Offscreen document",
    );
    assertEquals(
      apis.scripts[0].id,
      "worker-native",
      "Worker registers actual userScript",
    );
    const declarative = (await command("declarative")).result;
    assert(
      declarative.requiresNew &&
        declarative.matcherInstance &&
        declarative.showInstance &&
        declarative.iconPlain,
      "native declarative constructors preserve measured Chrome new/prototype contracts",
    );
    assertEquals(
      declarative.legacyType,
      "declarativeContent.ShowAction",
      "legacy ShowPageAction uses canonical action type",
    );
    assertEquals(
      declarative.rules[0].id,
      "worker-native-declarative",
      "Worker declarative rules reach actual native store",
    );
    target.linkedBrowser.reload();
    await new Promise((resolve) => setTimeout(resolve, 300));
    const tabId = extension.tabManager.getWrapper(target).id;
    const scripting = (await command("scripting", { tabId })).result;
    assertEquals(
      scripting.result[0].result.sum,
      5,
      "Worker serializes callable and JSON args into native executeScript",
    );
    assertEquals(
      scripting.result[0].result.userScript,
      "native",
      "registered userScript executed in target document",
    );
    assertEquals(
      scripting.result[0].result.declarative,
      "native",
      "Worker declarative content action executes actual extension file",
    );
    assertEquals(
      scripting.file[0].result.file,
      "native-file",
      "Worker executes extension script file",
    );
    assert(
      scripting.callbackError && scripting.cyclicRejected,
      "callback lastError and cyclic argument rejection propagate",
    );
    const additional = (
      await command("additional", {
        tabId,
        url: fixtureURL + "worker-target.html",
      })
    ).result;
    assert(
      additional.permissions.permissions.includes("cookies"),
      "permissions.getAll reaches real extension permissions",
    );
    assertEquals(
      additional.self.id,
      id,
      "management.getSelf returns installed extension",
    );
    assertEquals(
      additional.redirect,
      `https://${originalId}.chromiumapp.org/native-worker`,
      "identity redirect uses original Chrome identity",
    );
    assertEquals(
      additional.readCookie.value,
      "real",
      "Worker cookies backend stores and reads actual cookie",
    );
    assert(
      typeof additional.privacy.levelOfControl === "string",
      "Worker privacy setting returns native control state",
    );
    assert(
      Array.isArray(additional.downloads),
      "Worker downloads.search returns backend results",
    );
    assert(
      additional.frames.some(
        (frame: { frameId: number }) => frame.frameId === 0,
      ),
      "Worker webNavigation sees native top frame",
    );
    assertEquals(
      additional.menuId,
      "worker-menu",
      "contextMenus.create returns ID synchronously",
    );
    assertEquals(
      additional.menuCallback,
      0,
      "contextMenus callback receives no synthetic result argument",
    );
    assert(
      typeof additional.duplicateMenuError === "string",
      "contextMenus duplicate ID reports real callback lastError",
    );
    assertEquals(
      additional.group.title,
      "Worker native group",
      "tabGroups changes actual native group",
    );
    const auxiliary = (await command("auxiliary")).result;
    assert(
      ["granted", "denied"].includes(auxiliary.notificationPermission),
      "native notifications permission level is available in Worker",
    );
    assert(
      auxiliary.fontCount > 0,
      "Worker fontSettings enumerates actual installed fonts",
    );
    assertEquals(
      auxiliary.changed.pixelSize,
      auxiliary.before.pixelSize + 1,
      "Worker fontSettings changes native preference",
    );
    assertEquals(
      auxiliary.fontChange.pixelSize,
      auxiliary.changed.pixelSize,
      "Worker receives native font change event",
    );
    assert(
      auxiliary.voiceCount > 0,
      "Worker TTS enumerates actual installed voices",
    );
    assert(
      auxiliary.events.some(
        (event: { type: string }) => event.type === "start",
      ),
      "Worker TTS nested callback receives actual start event",
    );
    assertEquals(
      auxiliary.events.at(-1).type,
      "end",
      "Worker TTS nested callback reaches native final event",
    );
    assert(
      auxiliary.events.every(
        (event: Record<string, unknown>) =>
          !("srcId" in event) && !("isFinalEvent" in event),
      ),
      "Worker TTS hides internal routing fields",
    );
    assert(
      (await command("callback-message")).result.callback,
      "runtime onMessage sendResponse callback works",
    );
    const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
      Ci.nsIServiceWorkerManager,
    );
    const worker = swm.getRegistrationForAddonPrincipal(
      extension.principal,
    ).activeWorker;
    const workerId = worker.id;
    await extension.terminateBackground();
    await waitFor(
      () => extension.backgroundState === "stopped",
      "Worker terminated",
    );
    const restarted = (await command("probe")).result;
    assertEquals(
      restarted.starts,
      probe.starts + 1,
      "runtime message wakes stopped Worker and retains local storage",
    );
    assertEquals(
      swm.getRegistrationForAddonPrincipal(extension.principal).activeWorker.id,
      workerId,
      "idle stop preserves actual ServiceWorker registration",
    );
    assertEquals(
      (await command("session")).result.sessionValue,
      "retained",
      "session storage survives Worker restart",
    );
  } finally {
    if (control) win.gBrowser.removeTab(control);
    await addon?.uninstall();
    win.gBrowser.removeTab(target);
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}

async function testWorkerPermissions(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "normal Firefox Worker preference remains disabled",
  );
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const target = win.gBrowser.addTab(
    win.wrappedJSObject.__floorpRuntimeFixtureURL + "worker-target.html",
    { triggeringPrincipal: principal },
  );
  await waitFor(
    () => target.linkedBrowser.contentTitle === "Worker injection target",
    "permission test target",
  );
  const originalId = "iiiiiiiijjjjjjjjkkkkkkkkllllllll";
  const id = `${originalId}@chromium-extension`;
  const manifest = {
    manifest_version: 3,
    name: "Worker permission regression fixture",
    version: "1.0",
    permissions: ["scripting"],
    background: { service_worker: "worker.js" },
    browser_specific_settings: {
      gecko: {
        id,
        floorp_chrome_extension: { schema_version: 1, original_id: originalId },
      },
    },
  };
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "worker.js": `
      browser.runtime.onMessage.addListener(async message=>{
        let storageError,hostError;
        try {await browser.storage.local.get(null);}catch(error){storageError=error.message;}
        try {await browser.scripting.executeScript({target:{tabId:message.tabId},func:()=>1});}catch(error){hostError=error.message;}
        return {storageError,hostError};
      });
    `,
    "control.html": '<script src="control.js"></script>',
    "control.js":
      'document.title="ready";window.addEventListener("message",async event=>{try {document.title=JSON.stringify(await browser.runtime.sendMessage(event.data));}catch(error){document.title="FAIL:"+error.message;}});',
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-worker-permissions-${Services.uuid.generateUUID()}.xpi`,
  );
  await IOUtils.write(
    path,
    zipSync(
      Object.fromEntries(
        Object.entries(files).map(([name, text]) => [
          name,
          new TextEncoder().encode(text),
        ]),
      ),
    ),
  );
  let addon: { uninstall(): Promise<void> } | undefined;
  let control: typeof target | undefined;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "permission Worker extension",
    );
    control = win.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: principal,
    });
    await waitFor(
      () => control?.linkedBrowser.contentTitle === "ready",
      "permission control page",
    );
    const tabId = extension.tabManager.getWrapper(target).id;
    control.linkedBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(`content.postMessage({tabId:${tabId}},"*");`),
      false,
    );
    const result = await waitFor(() => {
      const title = control?.linkedBrowser.contentTitle;
      if (title?.startsWith("FAIL:")) throw new Error(title);
      return title?.startsWith("{") ? JSON.parse(title) : false;
    }, "native permission rejection results");
    assert(
      /Not enough privileges.*storage/i.test(result.storageError),
      "dotted storage namespace cannot bypass storage permission",
    );
    assert(
      /permission|cannot access|denied|not allowed/i.test(result.hostError),
      "scripting permission alone cannot inject without target host permission",
    );
  } finally {
    if (control) win.gBrowser.removeTab(control);
    await addon?.uninstall();
    win.gBrowser.removeTab(target);
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
  const ordinaryPath = PathUtils.join(
    PathUtils.tempDir,
    `floorp-ordinary-worker-${Services.uuid.generateUUID()}.xpi`,
  );
  await IOUtils.write(
    ordinaryPath,
    zipSync({
      "manifest.json": new TextEncoder().encode(
        JSON.stringify({
          manifest_version: 3,
          name: "Ordinary Firefox Worker",
          version: "1.0",
          background: { service_worker: "worker.js" },
          browser_specific_settings: {
            gecko: { id: "ordinary-worker@example.invalid" },
          },
        }),
      ),
      "worker.js": new TextEncoder().encode("self.started=true;"),
    }),
  );
  let ordinary: { uninstall(): Promise<void> } | undefined;
  let rejected = false;
  try {
    ordinary = await AddonManager.installTemporaryAddon(
      new FileUtils.File(ordinaryPath),
    );
  } catch {
    rejected = true;
  } finally {
    await ordinary?.uninstall();
    await IOUtils.remove(ordinaryPath, { ignoreAbsent: true });
  }
  assert(
    rejected,
    "ordinary Firefox extension does not gain opt-in Chrome Worker support",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("WorkerNative.test.mts", [
    {
      name:
        "native classic Worker imports, APIs, callbacks, storage and event restart",
      fn: () => testWorker("classic"),
    },
    {
      name:
        "native module Worker imports, APIs, callbacks, storage and event restart",
      fn: () => testWorker("module"),
    },
    {
      name:
        "native Worker storage, host and compatibility marker permission gates",
      fn: testWorkerPermissions,
    },
  ]);
}
