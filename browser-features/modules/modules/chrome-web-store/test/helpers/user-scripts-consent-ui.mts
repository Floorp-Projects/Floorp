// SPDX-License-Identifier: MPL-2.0
// Headed native UI only. This helper never sets a permission, preference, or
// the private consent flag. The production component owns the state change.
import {
  assert,
  assertEquals,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type {
  UserScriptsConsentResourceProtocol,
  UserScriptsConsentService,
  UserScriptsConsentStatus,
  UserScriptsConsentUICommand,
  UserScriptsConsentUIEvidence,
  UserScriptsTrustedInput,
  UserScriptsUIBrowser,
  UserScriptsUIReply,
  UserScriptsUIState,
  UserScriptsUITab,
  UserScriptsUIWindow,
} from "./user-scripts-consent-test-types.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
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
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw Error("Timed out waiting for " + description);
}

export function queryUserScriptsDocument<Result>(
  browser: UserScriptsUIBrowser,
  source: string,
): Promise<Result> {
  assert(
    browser.currentURI.spec !== "about:addons",
    "Add-ons UI uses only finite static actor commands",
  );
  const channel = "Floorp:UserScriptsConsent:" + Services.uuid.generateUUID();
  const manager = browser.messageManager;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      manager.removeMessageListener(channel, receive);
      reject(Error("User scripts consent document did not reply"));
    }, 8000);
    const receive = (message: UserScriptsUIReply<Result>) => {
      clearTimeout(timer);
      manager.removeMessageListener(channel, receive);
      if (message.data.error) reject(Error(message.data.error));
      else resolve(message.data.result as Result);
    };
    manager.addMessageListener(channel, receive);
    manager.loadFrameScript(
      "data:application/javascript," + encodeURIComponent(
        `(async()=>{try{const result=await (${source})();sendAsyncMessage(${
          JSON.stringify(channel)
        },{result});}catch(error){sendAsyncMessage(${
          JSON.stringify(channel)
        },{error:String(error)+error.stack});}})();`,
      ),
      false,
    );
  });
}

async function registerConsentActor(): Promise<() => void> {
  const directory = Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY");
  assert(
    directory,
    "copy the private consent actor with --actor-module before running this suite",
  );
  const actorPath = PathUtils.join(
    directory,
    "UserScriptsConsentUIChild.sys.mjs",
  );
  assert(
    await IOUtils.exists(actorPath),
    "the private consent actor was copied into this test profile",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const handler = Services.io.getProtocolHandler("resource");
  assert(handler?.QueryInterface, "native resource protocol is available");
  const resource = handler.QueryInterface(
    Ci.nsIResProtocolHandler,
  ) as unknown as UserScriptsConsentResourceProtocol;
  resource.setSubstitution(
    "floorp-user-scripts-consent-tests",
    Services.io.newFileURI(new FileUtils.File(directory)),
  );
  try {
    ChromeUtils.registerWindowActor("UserScriptsConsentUI", {
      child: {
        esModuleURI:
          "resource://floorp-user-scripts-consent-tests/UserScriptsConsentUIChild.sys.mjs",
      },
      matches: ["about:addons"],
      allFrames: false,
      includeChrome: false,
    });
  } catch (error) {
    resource.setSubstitution("floorp-user-scripts-consent-tests", null);
    throw error;
  }
  return () => {
    try {
      ChromeUtils.unregisterWindowActor("UserScriptsConsentUI");
    } finally {
      resource.setSubstitution("floorp-user-scripts-consent-tests", null);
    }
  };
}

function consentModule(): UserScriptsConsentService {
  const { ChromeExtensionUserScriptsConsent } = ChromeUtils.importESModule(
    "resource://gre/modules/ChromeExtensionUserScriptsConsent.sys.mjs",
  ) as unknown as {
    ChromeExtensionUserScriptsConsent: UserScriptsConsentService;
  };
  return ChromeExtensionUserScriptsConsent;
}

export async function readUserScriptsConsentStatus(
  id: string,
): Promise<UserScriptsConsentStatus> {
  return await consentModule().getStatus(id);
}

function queryConsentUI<Result>(
  browser: UserScriptsUIBrowser,
  name: UserScriptsConsentUICommand,
  id: string,
): Promise<Result> {
  const global = browser.browsingContext.currentWindowGlobal;
  assert(
    global?.documentURI.spec === "about:addons",
    "native Add-ons Manager document is current",
  );
  let timer = 0;
  const timeout = new Promise<Result>((_resolve, reject) => {
    timer = setTimeout(
      () => reject(Error("Native consent UI actor query timed out: " + name)),
      8000,
    );
  });
  try {
    return Promise.race([
      global.getActor("UserScriptsConsentUI").sendQuery<Result>(name, { id }),
      timeout,
    ]).finally(() => clearTimeout(timer));
  } catch (error) {
    clearTimeout(timer);
    throw error;
  }
}

async function withConsentPanel<Result>(
  id: string,
  window: UserScriptsUIWindow,
  inspect: (browser: UserScriptsUIBrowser) => Promise<Result>,
): Promise<Result> {
  assert(
    !Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo).isHeadless,
    "user scripts consent requires genuine headed native UI",
  );
  const previousTab = window.gBrowser.selectedTab;
  const unregisterActor = await registerConsentActor();
  let tab: UserScriptsUITab | undefined;
  try {
    tab = window.gBrowser.addTab("about:addons", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    const browser = tab.linkedBrowser;
    window.gBrowser.selectedTab = tab;
    window.focus();
    await until(
      () => Services.focus.activeWindow === window,
      "focused Add-ons Manager window",
    );
    await until(
      () =>
        browser.browsingContext.currentWindowGlobal?.documentURI.spec ===
          "about:addons",
      "current native Add-ons Manager document",
    );
    await until(
      () => queryConsentUI<boolean>(browser, "Initialized", id),
      "genuine Add-ons Manager initialization",
    );
    // Only navigation uses the view controller; permission changes always come
    // from actual trusted input on the visible native switch.
    await queryConsentUI(browser, "Navigate", id);
    await until(
      () => queryConsentUI<boolean>(browser, "PermissionsVisible", id),
      "visible native permissions tab",
    );
    const tabEvents = await queryConsentUI<UserScriptsTrustedInput[]>(
      browser,
      "ClickPermissions",
      id,
    );
    assert(
      tabEvents.some((event) => event.type === "click" && event.trusted),
      "native permissions tab receives a trusted click",
    );
    await until(
      () => queryConsentUI<boolean>(browser, "PanelVisible", id),
      "native permissions panel displayed",
    );
    return await inspect(browser);
  } finally {
    try {
      if (tab?.isConnected) window.gBrowser.removeTab(tab);
      if (previousTab?.isConnected) window.gBrowser.selectedTab = previousTab;
    } finally {
      unregisterActor();
    }
  }
}

export async function inspectUserScriptsConsentUI(
  id: string,
  window: UserScriptsUIWindow,
): Promise<UserScriptsUIState> {
  const status = await readUserScriptsConsentStatus(id);
  return await withConsentPanel(
    id,
    window,
    (browser) =>
      until(async () => {
        const state = await queryConsentUI<UserScriptsUIState>(
          browser,
          "Snapshot",
          id,
        );
        return (status.eligible
          ? state.toggleVisible && !state.disabled
          : !state.componentPresent || state.componentHidden) && state;
      }, "consent component settled to its eligible or ordinary-addon state"),
  );
}

export async function setUserScriptsConsentUsingUI(
  id: string,
  allowed: boolean,
  window: UserScriptsUIWindow,
): Promise<UserScriptsConsentUIEvidence> {
  return await withConsentPanel(id, window, async (browser) => {
    const statusBefore = await readUserScriptsConsentStatus(id);
    assert(
      statusBefore.eligible,
      "only converted eligible addons have this consent",
    );
    assertEquals(
      statusBefore.allowed,
      !allowed,
      "the real UI must change consent",
    );
    const before = await until(async () => {
      const state = await queryConsentUI<UserScriptsUIState>(
        browser,
        "Snapshot",
        id,
      );
      return state.toggleVisible && !state.disabled && Boolean(state.label) &&
        state;
    }, "visible enabled Allow user scripts switch");
    assertEquals(
      before.pressed,
      statusBefore.allowed,
      "visible consent matches saved state",
    );
    assert(
      before.url.startsWith("about:addons"),
      "consent is shown in the real Add-ons Manager",
    );
    assertEquals(
      before.addonId,
      id,
      "visible consent belongs to the selected native addon card",
    );
    const nativeEvents = await queryConsentUI<UserScriptsTrustedInput[]>(
      browser,
      "ClickToggle",
      id,
    );
    assert(
      nativeEvents.some((event) => event.type === "click" && event.trusted),
      "Allow user scripts receives a trusted native click",
    );
    const changed = await until(async () => {
      const status = await readUserScriptsConsentStatus(id);
      const state = await queryConsentUI<UserScriptsUIState>(
        browser,
        "Snapshot",
        id,
      );
      return status.allowed === allowed && state.pressed === allowed &&
        !state.disabled && { status, state };
    }, "native consent component persisted the requested state");
    return {
      addonId: id,
      requestedAllowed: allowed,
      before,
      after: changed.state,
      statusBefore,
      statusAfter: changed.status,
      nativeEvents,
    };
  });
}
