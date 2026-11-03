// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
/// <reference path="../../../../../libs/@types/gecko/index.d.ts" />
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  NativeURLExtension,
  NativeURLResult,
  NativeURLTab,
  NativeURLWindow,
} from "./helpers/public-extension-url-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const ID = "f".repeat(32);
const FOREIGN = "e".repeat(32);
const probe = String.raw`async function(config) {
  const own = 'chrome-extension://' + chrome.runtime.id + '/';
  const foreign = 'chrome-extension://' + ${JSON.stringify(FOREIGN)} + '/';
  const paths = ["", "foo", "/foo", "//other.example/path", "https://example.invalid/a?x=1#part", "?q=one#part", "#fragment", "nested/../foo", "../../escape", "space name.txt", "%2e%2e/escape", "a\\b", "chrome-extension://eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee/private.txt", "chrome-extension://{OWN_ID}/foo"].map(path=>path.replaceAll("{OWN_ID}",chrome.runtime.id));
  const detached = chrome.runtime.getURL;
  const value = {id:chrome.runtime.id,worker:typeof ServiceWorkerGlobalScope !== 'undefined' && self instanceof ServiceWorkerGlobalScope,
    urls:paths.map(path=>chrome.runtime.getURL(path)), detached:detached('foo'),foreignThis:detached.call({}, 'foo'),
    legacyType:typeof chrome.extension?.getURL};
  if(config.mv===2 && typeof chrome.extension?.getURL==='function') value.legacy=chrome.extension.getURL('foo');
  value.body=await (await fetch(chrome.runtime.getURL('private.txt?query=one#fragment'))).text();
  if(config.compatible) {
    const action = config.mv===2 ? chrome.browserAction : chrome.action;
    const actionCall=(method,details)=>config.mv===2 ? new Promise((resolve,reject)=>action[method](details,response=>{const error=chrome.runtime.lastError;if(error)reject(Error(error.message));else resolve(response);})) : action[method](details);
    await actionCall('setPopup',{popup:chrome.runtime.getURL('popup.html')});
    value.popup=await actionCall('getPopup',{});
    for(const [name,popup] of [
      ['foreignPopupDenied',foreign+'popup.html'],
      ['credentialPopupDenied','chrome-extension://user@'+chrome.runtime.id+'/popup.html'],
      ['portPopupDenied','chrome-extension://'+chrome.runtime.id+':9876/popup.html']]) {
      try {await actionCall('setPopup',{popup});value[name]=false;} catch {value[name]=true;}
    }
    await actionCall('setPopup',{popup:''});
    if(config.mv===3) {
      await chrome.offscreen.createDocument({url:chrome.runtime.getURL('offscreen.html'),reasons:['DOM_PARSER'],justification:'Own alias must retain native origin'});
      try {value.offscreen=await chrome.offscreen.hasDocument();}finally {await chrome.offscreen.closeDocument();}
      try {await chrome.offscreen.createDocument({url:foreign+'offscreen.html',reasons:['DOM_PARSER'],justification:'Foreign alias denial'});value.foreignOffscreenDenied=false;}
      catch {value.foreignOffscreenDenied=true;}
      if(await chrome.offscreen.hasDocument())await chrome.offscreen.closeDocument();
      await chrome.sidePanel.setOptions({path:chrome.runtime.getURL('popup.html'),enabled:true});
      value.sidePanel=await chrome.sidePanel.getOptions({});
      try {await chrome.sidePanel.setOptions({path:foreign+'popup.html',enabled:true});value.foreignSidePanelDenied=false;}
      catch {value.foreignSidePanelDenied=true;}
    }
    if(config.kind==='classic') value.imported=self.authoredOwnAliasImport;
    if(config.kind==='module') value.staticImported=self.authoredOwnAliasImport;
  }
  value.externalSender=await new Promise((resolve,reject)=>chrome.runtime.sendMessage(config.compatible ? ${
  JSON.stringify(FOREIGN)
} : ${
  JSON.stringify(FOREIGN + "@chromium-extension")
}, {authoredSenderSnapshot:true}, response=>{const error=chrome.runtime.lastError;if(error)reject(Error(error.message));else resolve(response);}));
  return value;
}`;
async function waitFor<T>(
  read: () => T | false | undefined,
  label: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw Error("Timed out waiting for " + label);
}
async function testPublicURL(
  kind: "document" | "classic" | "module",
  mv: 2 | 3,
  compatible = true,
): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  if (kind !== "document") {
    assert(
      AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
      "Actual compiled Worker binding required",
    );
  }
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as NativeURLWindow;
  const id = compatible
    ? `${ID}@chromium-extension`
    : "ordinary-public-url@floorp-test";
  const manifest = {
    manifest_version: mv,
    name: "Authored public extension URL " + kind,
    version: "1.0",
    permissions: compatible && mv === 3 ? ["offscreen", "sidePanel"] : [],
    ...(mv === 2 ? { browser_action: {} } : { action: {} }),
    ...(kind === "document" ? {} : {
      background: {
        service_worker: "worker.js",
        ...(kind === "module" ? { type: "module" } : {}),
      },
    }),
    browser_specific_settings: {
      gecko: {
        id,
        ...(compatible
          ? { floorp_chrome_extension: { schema_version: 1, original_id: ID } }
          : {}),
      },
    },
  };
  const requestId = Services.uuid.generateUUID().toString();
  const config = { kind, mv, compatible, requestId };
  const control = kind === "document"
    ? `(${probe})(${JSON.stringify(config)})`
    : `chrome.runtime.sendMessage(${JSON.stringify(config)})`;
  const files: Record<string, string> = {
    "manifest.json": JSON.stringify(manifest),
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js":
      `document.title='authored-ready';window.addEventListener('message',async event=>{if(event.data?.requestId!==${
        JSON.stringify(requestId)
      })return;try{document.title=JSON.stringify({requestId:event.data.requestId,value:await ${control}});}catch(error){document.title=JSON.stringify({requestId:event.data.requestId,error:String(error)});}});`,
    "worker.js":
      (kind === "classic"
        ? `importScripts(chrome.runtime.getURL("imported.js"));
`
        : kind === "module"
        ? `import { authoredOwnAliasImport } from "chrome-extension://${ID}/imported.mjs";
globalThis.authoredOwnAliasImport=authoredOwnAliasImport;
`
        : "") +
      `chrome.runtime.onMessage.addListener((message,sender)=>message.requestId===${
        JSON.stringify(requestId)
      }?(${probe})(message).then(value=>({...value,sender})):undefined);`,
    "private.txt": "owned private payload",
    "popup.html": "<!doctype html><title>Owned popup</title>",
    "offscreen.html": "<!doctype html><title>Owned DOM parser</title>",
    "imported.js": 'self.authoredOwnAliasImport="real-own-alias-import";',
    "imported.mjs":
      'export const authoredOwnAliasImport="real-own-alias-import";',
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    "floorp-public-url-" + requestId + ".xpi",
  );
  await IOUtils.write(
    path,
    zipSync(
      Object.fromEntries(
        Object.entries(files).map((
          [name, value],
        ) => [name, new TextEncoder().encode(value)]),
      ),
    ),
  );
  const foreignPath = PathUtils.join(
    PathUtils.tempDir,
    "floorp-public-url-foreign-" + requestId + ".xpi",
  );
  await IOUtils.write(
    foreignPath,
    zipSync({
      "manifest.json": new TextEncoder().encode(JSON.stringify({
        manifest_version: 3,
        name: "Installed foreign public URL control",
        version: "1.0",
        background: compatible && mv === 3 && kind !== "classic"
          ? {
            service_worker: "receiver.js",
            ...(kind === "module" ? { type: "module" } : {}),
          }
          : { scripts: ["receiver.js"] },
        web_accessible_resources: [{
          resources: ["popup.html", "offscreen.html"],
          matches: ["<all_urls>"],
        }],
        browser_specific_settings: {
          gecko: {
            id: `${FOREIGN}@chromium-extension`,
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: FOREIGN,
            },
          },
        },
      })),
      "receiver.js": new TextEncoder().encode(
        "chrome.runtime.onMessageExternal.addListener((message,sender)=>message.authoredSenderSnapshot ? Promise.resolve({id:sender.id,url:sender.url,origin:sender.origin,receiverWorker:typeof ServiceWorkerGlobalScope!=='undefined'&&self instanceof ServiceWorkerGlobalScope}):undefined);",
      ),
      "popup.html": new TextEncoder().encode(
        "<!doctype html><title>Installed foreign popup</title>",
      ),
      "offscreen.html": new TextEncoder().encode(
        "<!doctype html><title>Installed foreign offscreen</title>",
      ),
    }),
  );
  let tab: NativeURLTab | undefined;
  let addon: { uninstall(): Promise<void> } | undefined;
  let foreignAddon: { uninstall(): Promise<void> } | undefined;
  try {
    foreignAddon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(foreignPath),
    );
    await waitFor(
      () =>
        ExtensionParent.GlobalManager.extensionMap.has(
          `${FOREIGN}@chromium-extension`,
        ),
      "actual installed foreign extension",
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () =>
        ExtensionParent.GlobalManager.extensionMap.get(id) as
          | NativeURLExtension
          | undefined,
      "actual installed extension",
    );
    if (kind !== "document") {
      await waitFor(
        () => extension.backgroundContext?.viewType === "background_worker",
        "actual native " + kind + " Worker",
      );
      assert(
        extension.policy.backgroundServiceWorkerAllowed,
        "Only marked Worker policy enabled",
      );
      assertEquals(
        extension.policy.backgroundTypeModule,
        kind === "module",
        "Actual native module selection",
      );
    }
    tab = win.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await waitFor(
      () => tab?.linkedBrowser.contentTitle === "authored-ready",
      "actual extension DOC readiness",
    );
    tab.linkedBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `content.postMessage(${JSON.stringify({ requestId })},'*')`,
        ),
      false,
    );
    const reply = await waitFor(() => {
      try {
        const value = JSON.parse(
          tab!.linkedBrowser.contentTitle,
        ) as NativeURLResult;
        return value.requestId === requestId ? value : false;
      } catch {
        return false;
      }
    }, "correlated public getURL response");
    assert(
      !reply.error,
      reply.error ?? "Public URLs load with native principals",
    );
    const value = reply.value;
    assert(value, "Actual public API result");
    assertEquals(value.worker, kind !== "document", "Actual execution realm");
    const base = compatible
      ? `chrome-extension://${ID}/`
      : extension.baseURI.spec;
    assertEquals(value.urls[0], base, "Public extension root");
    if (compatible) {
      assertEquals(
        JSON.stringify(value.urls),
        JSON.stringify([
          "chrome-extension://ffffffffffffffffffffffffffffffff/",
          "chrome-extension://ffffffffffffffffffffffffffffffff/foo",
          "chrome-extension://ffffffffffffffffffffffffffffffff/foo",
          "chrome-extension://ffffffffffffffffffffffffffffffff//other.example/path",
          "chrome-extension://ffffffffffffffffffffffffffffffff/https://example.invalid/a?x=1#part",
          "chrome-extension://ffffffffffffffffffffffffffffffff/?q=one#part",
          "chrome-extension://ffffffffffffffffffffffffffffffff/#fragment",
          "chrome-extension://ffffffffffffffffffffffffffffffff/foo",
          "chrome-extension://ffffffffffffffffffffffffffffffff/escape",
          "chrome-extension://ffffffffffffffffffffffffffffffff/space%20name.txt",
          "chrome-extension://ffffffffffffffffffffffffffffffff/escape",
          "chrome-extension://ffffffffffffffffffffffffffffffff/a/b",
          "chrome-extension://ffffffffffffffffffffffffffffffff/chrome-extension://eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee/private.txt",
          "chrome-extension://ffffffffffffffffffffffffffffffff/chrome-extension://ffffffffffffffffffffffffffffffff/foo",
        ]),
        "Chrome fixed-prefix source vectors",
      );
    }
    assertEquals(value.detached, base + "foo", "Detached getURL works");
    assertEquals(
      value.foreignThis,
      base + "foo",
      "Foreign this cannot change owner",
    );
    assertEquals(
      value.body,
      "owned private payload",
      "Own alias fetch reads real private file",
    );
    assertEquals(
      value.externalSender?.id,
      compatible ? ID : id,
      "External sender identity depends on authenticated source and receiver",
    );
    assertEquals(
      value.externalSender?.url,
      compatible
        ? base + (kind === "document" ? "control.html" : "worker.js")
        : extension.baseURI.resolve("control.html"),
      "External DOC/Worker sender public URL is source-owned",
    );
    assertEquals(
      value.externalSender?.origin,
      extension.baseURI.prePath,
      "External Worker origin belongs to source, never receiver",
    );
    assertEquals(
      value.externalSender?.receiverWorker,
      compatible && mv === 3 && kind !== "classic",
      "External delivery reaches genuine receiver realm",
    );
    const principal = tab.linkedBrowser.browsingContext.currentWindowGlobal
      ?.documentPrincipal;
    assertEquals(
      principal?.addonId,
      id,
      "Public URL does not replace native document principal",
    );
    assert(
      principal?.URI?.schemeIs("moz-extension"),
      "Internal document URI remains native",
    );
    assert(
      extension.baseURI.schemeIs("moz-extension"),
      "Internal extension base URI remains native",
    );
    if (compatible) {
      assertEquals(value.id, ID, "Public identity remains original32");
      if (kind !== "document") {
        assertEquals(value.sender?.id, ID, "Own DOC sender public identity");
        assertEquals(
          value.sender?.url,
          base + "control.html",
          "Own DOC sender URL presentation",
        );
        assertEquals(
          value.sender?.origin,
          extension.baseURI.prePath,
          "Sender actual native origin is not rewritten",
        );
      }
      assertEquals(
        value.popup,
        base + "popup.html",
        "Own absolute popup is accepted and presented publicly",
      );
      assert(
        value.foreignPopupDenied && value.credentialPopupDenied &&
          value.portPopupDenied,
        "Foreign and malformed resource aliases remain denied",
      );
      if (mv === 2) {
        assertEquals(
          value.legacy,
          base + "foo",
          "Existing legacy DOC method shares public presentation",
        );
      }
      if (mv === 3) {
        assert(
          value.offscreen && value.foreignOffscreenDenied &&
            value.foreignSidePanelDenied,
          "Own offscreen works; foreign resources remain denied",
        );
      }
      if (mv === 3) {
        assertEquals(
          value.sidePanel?.path,
          base + "popup.html",
          "Side panel keeps caller public path",
        );
      }
      if (kind === "classic") {
        assertEquals(
          value.imported,
          "real-own-alias-import",
          "Actual Worker importScripts loads own alias",
        );
      }
      if (kind === "module") {
        assertEquals(
          value.staticImported,
          "real-own-alias-import",
          "Actual module Worker static import loads own alias",
        );
      }
    } else assertEquals(value.id, id, "Ordinary Firefox identity unchanged");
  } finally {
    if (tab) win.gBrowser.removeTab(tab);
    await addon?.uninstall();
    await foreignAddon?.uninstall();
    await IOUtils.remove(path);
    await IOUtils.remove(foreignPath);
  }
}
export async function runAllTests(): Promise<void> {
  await runTests(
    "Public Chrome extension URLs, native resources and principals",
    [
      {
        name: "marked MV2 DOC legacy and runtime public URLs",
        fn: () => testPublicURL("document", 2),
      },
      {
        name: "marked MV3 DOC public URLs and real consumers",
        fn: () => testPublicURL("document", 3),
      },
      {
        name: "actual classic Worker public URLs/importScripts and consumers",
        fn: () => testPublicURL("classic", 3),
      },
      {
        name: "actual module Worker public URLs/static import and consumers",
        fn: () => testPublicURL("module", 3),
      },
      {
        name: "ordinary Firefox native URL and principal control",
        fn: () => testPublicURL("document", 3, false),
      },
    ],
  );
}
