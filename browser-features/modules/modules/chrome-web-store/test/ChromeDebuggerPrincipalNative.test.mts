// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import type {
  TestTab,
  TestWindow,
} from "./helpers/debugger-auxiliary-types.ts";
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function waitFor<T>(
  read: () => T | false | undefined,
  label: string,
): Promise<T> {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw Error("Timed out waiting for " + label);
}
async function testInheritedAddonPrincipal(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const aId = "debugger-principal-owner@example.invalid",
    bId = "gh".repeat(16),
    bGecko = bId + "@chromium-extension";
  const encode = (text: string) => new TextEncoder().encode(text);
  const paths: string[] = [];
  const addons: Awaited<
    ReturnType<typeof AddonManager.installTemporaryAddon>
  >[] = [];
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as TestWindow;
  const original = new Set(win.gBrowser.tabs);
  try {
    for (
      const [name, manifest, files] of [
        ["owner", {
          manifest_version: 2,
          name: "Owned extension inherited principal",
          version: "1.0",
          browser_specific_settings: { gecko: { id: aId } },
        }, {
          "opener.html":
            "<!doctype html><title>Owned opener ready</title><button id=open style='width:200px;height:80px'>Open inherited document</button><script src=opener.js></script>",
          "opener.js":
            "document.querySelector('#open').onclick=()=>{const child=window.open('about:blank','_blank');if(child){child.privateAddonSecret='authored-other-addon-secret-73195';child.document.title='Inherited addon principal target';document.title='Owned opener completed'}};",
        }],
        ["debugger", {
          manifest_version: 3,
          name: "Different compatible debugger owner",
          version: "1.0",
          permissions: ["debugger"],
          background: { scripts: ["background.js"] },
          browser_specific_settings: {
            gecko: {
              id: bGecko,
              floorp_chrome_extension: { schema_version: 1, original_id: bId },
            },
          },
        }, {
          "background.js":
            `document.title='debugger-ready';window.addEventListener('message',async({data})=>{if(!data?.principalProbe)return;const source={tabId:data.tabId};let result;try{await browser.debugger.attach(source,'1.3');const value=await browser.debugger.sendCommand(source,'Runtime.evaluate',{expression:'globalThis.privateAddonSecret',returnByValue:true});result={allowed:true,value:value.result.value};await browser.debugger.detach(source)}catch(error){result={allowed:false,error:String(error)}}document.title=JSON.stringify(result)})`,
        }],
      ] as const
    ) {
      const path = PathUtils.join(
        PathUtils.tempDir,
        `floorp-debugger-principal-${name}-${Services.uuid.generateUUID()}.xpi`,
      );
      paths.push(path);
      await IOUtils.write(
        path,
        zipSync(
          Object.fromEntries(
            Object.entries({
              "manifest.json": JSON.stringify(manifest),
              ...files,
            }).map(([file, text]) => [file, encode(text)]),
          ),
        ),
      );
      addons.push(
        await AddonManager.installTemporaryAddon(new FileUtils.File(path)),
      );
    }
    const owner = ExtensionParent.GlobalManager.extensionMap.get(aId);
    const debuggerExtension = ExtensionParent.GlobalManager.extensionMap.get(
      bGecko,
    );
    assert(owner && debuggerExtension, "both actual extensions are installed");
    await waitFor(
      () =>
        debuggerExtension.backgroundContext?.xulBrowser.contentTitle ===
          "debugger-ready",
      "debugger background",
    );
    const opener = win.gBrowser.addTab(owner.baseURI.resolve("opener.html"), {
      triggeringPrincipal: owner.principal,
    });
    win.gBrowser.selectedTab = opener;
    await waitFor(
      () => opener.linkedBrowser.contentTitle === "Owned opener ready",
      "owner extension opener",
    );
    opener.linkedBrowser.messageManager.loadFrameScript(
      `data:application/javascript,${
        encodeURIComponent(
          "const button=content.document.querySelector('#open');const rect=button.getBoundingClientRect();content.synthesizeMouseEvent('mousedown',rect.x+rect.width/2,rect.y+rect.height/2,{button:0,buttons:1,clickCount:1});content.synthesizeMouseEvent('mouseup',rect.x+rect.width/2,rect.y+rect.height/2,{button:0,buttons:0,clickCount:1});",
        )
      }`,
      false,
    );
    const inherited = await waitFor(
      () =>
        [...win.gBrowser.tabs].find((tab) =>
          tab.linkedBrowser.contentTitle === "Inherited addon principal target"
        ),
      "genuine extension-opened inherited about:blank document",
    );
    assertEquals(
      inherited.linkedBrowser.currentURI.spec,
      "about:blank",
      "actual target URL is otherwise allowed",
    );
    assertEquals(
      inherited.linkedBrowser.browsingContext.currentWindowGlobal
        .documentPrincipal.addonId,
      aId,
      "native document principal belongs to the other extension",
    );
    debuggerExtension.backgroundContext.xulBrowser.messageManager
      .loadFrameScript(
        `data:application/javascript,${
          encodeURIComponent(
            `content.postMessage(${
              JSON.stringify({
                principalProbe: true,
                tabId: ExtensionParent.apiManager.global.tabTracker.getId(
                  inherited,
                ),
              })
            },'*')`,
          )
        }`,
        false,
      );
    const result = await waitFor(() => {
      try {
        const value = JSON.parse(
          debuggerExtension.backgroundContext.xulBrowser.contentTitle,
        ) as { allowed: boolean; value?: string; error?: string };
        return typeof value.allowed === "boolean" && value;
      } catch {
        return false;
      }
    }, "actual public debugger attach decision");
    console.log("[Debugger principal probe] " + JSON.stringify(result));
    assertEquals(
      result.allowed,
      false,
      "debugger cannot read another add-on's inherited principal: " +
        JSON.stringify(result),
    );
  } finally {
    for (const addon of addons.reverse()) await addon.uninstall();
    for (const tab of [...win.gBrowser.tabs]) {
      if (!original.has(tab)) {
        win.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    for (const path of paths) {
      await IOUtils.remove(path, { ignoreAbsent: true });
    }
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("Chrome debugger native principal isolation", [{
    name: "Different addon inherited about:blank remains protected",
    fn: testInheritedAddonPrincipal,
  }]);
}
