// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import type {
  FixtureState,
  NativeTab,
  NativeWindow,
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
  read: () => T | false | undefined | Promise<T | false | undefined>,
  label: string,
): Promise<T> {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${label}`);
}

async function testDeclarativeLifetimes(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { ExtensionPermissions } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionPermissions.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const id = "ef".repeat(16), geckoId = `${id}@chromium-extension`;
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-declarative-${Services.uuid.generateUUID()}.xpi`,
  );
  const persisted = PathUtils.join(
    PathUtils.profileDir,
    "extension-store",
    "chrome-declarative-content",
    id + ".json",
  );
  const permission = { permissions: ["declarativeContent"], origins: [] };
  const encode = (value: string) => new TextEncoder().encode(value);
  await IOUtils.write(
    path,
    zipSync({
      "manifest.json": encode(JSON.stringify({
        manifest_version: 3,
        name: "Native declarative lifecycle fixture",
        version: "1.0",
        permissions: ["tabs", "scripting"],
        optional_permissions: ["declarativeContent"],
        host_permissions: ["http://127.0.0.1/*"],
        action: {},
        background: { scripts: ["background.js"] },
        browser_specific_settings: {
          gecko: {
            id: geckoId,
            floorp_chrome_extension: { schema_version: 1, original_id: id },
          },
        },
      })),
      "script.js": encode(
        'document.title="Native declarative script injected";',
      ),
      "background.js": encode(`
      const state={ready:true};const publish=()=>document.title=JSON.stringify(state);
      window.addEventListener("message",async({data})=>{
        if(!data?.floorpDeclarativeFixture)return;
        try{
          if(data.method==="add")state.value=await browser.declarativeContent.onPageChanged.addRules([{id:"persisted-native-rule",conditions:[new browser.declarativeContent.PageStateMatcher({pageUrl:{queryContains:"match"},css:["body"]})],actions:[new browser.declarativeContent.ShowAction(),new browser.declarativeContent.RequestContentScript({js:["script.js"]})]}]);
          else if(data.method==="get")state.value=await browser.declarativeContent.onPageChanged.getRules();
          else if(data.method==="remove")state.value=await browser.declarativeContent.onPageChanged.removeRules();
          else if(data.method==="disable")state.value=await browser.action.disable();
          else if(data.method==="unmatch")state.value=await browser.scripting.executeScript({target:{tabId:data.tabId},func:()=>history.replaceState({},"","?other")});
          else if(data.method==="rematch")state.value=await browser.scripting.executeScript({target:{tabId:data.tabId},func:()=>history.replaceState({},"","?match")});
          delete state.error;
        }catch(error){state.error=String(error)}state.sequence=data.sequence;publish();
      });publish();
    `),
    }),
  );
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as NativeWindow;
  const system = Services.scriptSecurityManager.getSystemPrincipal();
  let addon, tab: NativeTab | undefined;
  let extension: ReturnType<
    typeof ExtensionParent.GlobalManager.extensionMap.get
  >;
  const read = (): FixtureState | undefined => {
    try {
      return JSON.parse(
        extension?.backgroundContext?.xulBrowser.contentTitle,
      ) as FixtureState;
    } catch {
      return undefined;
    }
  };
  let sequence = 0;
  const command = async (method: string) => {
    const current = ++sequence;
    extension.backgroundContext.xulBrowser.messageManager.loadFrameScript(
      `data:application/javascript,${
        encodeURIComponent(
          `content.postMessage(${
            JSON.stringify({
              floorpDeclarativeFixture: true,
              method,
              sequence: current,
              tabId: tab &&
                ExtensionParent.apiManager.global.tabTracker.getId(tab),
            })
          },"*")`,
        )
      }`,
      false,
    );
    const result = await waitFor(
      () => read()?.sequence === current && read(),
      method + " native callback",
    );
    assert(!result.error, result.error || "declarative call succeeds");
    return result.value;
  };
  try {
    await ExtensionPermissions.add(geckoId, permission);
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(geckoId),
      "fixture startup",
    );
    await waitFor(() => read()?.ready, "declarative listener");
    const url = win.wrappedJSObject.__floorpRuntimeFixtureURL +
      "worker-target.html?match";
    tab = win.gBrowser.addTab(url, { triggeringPrincipal: system });
    win.gBrowser.selectedTab = tab;
    await waitFor(
      () => tab?.linkedBrowser.contentTitle === "Worker injection target",
      "HTTP target",
    );
    await command("disable");
    await command("add");
    await waitFor(
      () =>
        tab?.linkedBrowser.contentTitle ===
          "Native declarative script injected",
      "actual authorized file injection",
    );
    let action =
      ExtensionParent.apiManager.getAPI("browserAction", extension).action;
    assertEquals(
      action.getContextData(tab).enabled,
      false,
      "imperative disabled state remains stored",
    );
    assertEquals(
      action.getProperty(tab, "enabled"),
      true,
      "native action is shown by the matching rule",
    );
    assert(
      await IOUtils.exists(persisted),
      "actual rules persist in the extension's profile store",
    );
    const firstDocument =
      tab.linkedBrowser.browsingContext.currentWindowGlobal.innerWindowId;
    await command("unmatch");
    await waitFor(
      () => action.getProperty(tab, "enabled") === false,
      "same-document URL change removes rule effects",
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal.innerWindowId,
      firstDocument,
      "history state change preserves the actual document",
    );
    await command("rematch");
    await waitFor(
      () => action.getProperty(tab, "enabled") === true,
      "same-document URL change rematches rules",
    );
    await extension.terminateBackground();
    tab.linkedBrowser.loadURI(Services.io.newURI(url + "&fresh"), {
      triggeringPrincipal: system,
    });
    await waitFor(
      () =>
        tab?.linkedBrowser.browsingContext.currentWindowGlobal.innerWindowId !==
          firstDocument &&
        tab?.linkedBrowser.contentTitle ===
          "Native declarative script injected",
      "parent rule operates with background stopped",
    );
    assert(
      !extension.backgroundContext,
      "rule matching and script execution do not wake the background",
    );
    tab.linkedBrowser.loadURI(
      Services.io.newURI(url.replace("?match", "?other")),
      { triggeringPrincipal: system },
    );
    await waitFor(
      () =>
        tab?.linkedBrowser.contentTitle === "Worker injection target" &&
        action.getProperty(tab, "enabled") === false,
      "navigation removes declarative action overlay",
    );
    const oldExtension = extension;
    await addon.reload();
    extension = await waitFor(() => {
      const value = ExtensionParent.GlobalManager.extensionMap.get(geckoId);
      return value && value !== oldExtension && value;
    }, "extension reload");
    await waitFor(() => read()?.ready, "reloaded declarative listener");
    const rules = await command("get") as { id: string }[];
    assertEquals(
      JSON.stringify(rules.map((rule) => rule.id)),
      JSON.stringify(["persisted-native-rule"]),
      "rules survive an actual extension reload",
    );
    await command("disable");
    action =
      ExtensionParent.apiManager.getAPI("browserAction", extension).action;
    tab.linkedBrowser.loadURI(Services.io.newURI(url), {
      triggeringPrincipal: system,
    });
    await waitFor(
      () =>
        tab?.linkedBrowser.contentTitle ===
          "Native declarative script injected" &&
        action.getProperty(tab, "enabled") === true,
      "persisted rule rematches after reload",
    );
    await ExtensionPermissions.remove(geckoId, permission, extension);
    await waitFor(
      () => action.getProperty(tab, "enabled") === false,
      "permission revocation clears native rule effects",
    );
    tab.linkedBrowser.loadURI(Services.io.newURI(url + "&revoked"), {
      triggeringPrincipal: system,
    });
    await waitFor(
      () => tab?.linkedBrowser.contentTitle === "Worker injection target",
      "revoked document loaded",
    );
    await new Promise((resolve) => setTimeout(resolve, 100));
    assertEquals(
      tab.linkedBrowser.contentTitle,
      "Worker injection target",
      "revoked declarative permission prevents real script injection",
    );
    await ExtensionPermissions.add(geckoId, permission, extension);
    await waitFor(
      () =>
        tab?.linkedBrowser.contentTitle ===
          "Native declarative script injected",
      "genuine permission restoration reactivates rules",
    );
    await command("remove");
    await waitFor(
      () => action.getProperty(tab, "enabled") === false,
      "rule removal restores imperative native state",
    );
    await addon.uninstall();
    addon = undefined;
    await waitFor(
      () => !extension.backgroundContext,
      "uninstall shuts down fixture",
    );
    await waitFor(
      async () => !(await IOUtils.exists(persisted)),
      "uninstall removes persisted declarative rules",
    );
  } finally {
    await addon?.uninstall();
    if (tab) win.gBrowser.removeTab(tab);
    await ExtensionPermissions.remove(geckoId, permission);
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("Chrome declarativeContent native lifecycle", [{
    name:
      "Actual rule matching, persistence, suspension, permission revocation and teardown",
    fn: testDeclarativeLifetimes,
  }]);
}
