/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import {
  effectiveReleaseNotesMode,
  RELEASE_NOTES_PREFS,
  releaseNotesAudience,
  shouldShowReleaseNotesChoice,
} from "../common/release-notes.ts";
import { launchDevPorts } from "../common/dev-port-offset.ts";
import {
  getComponentRegistrar,
  loadStartPageInPrivilegedAboutProcesses,
  registerCustomAboutPage,
  START_PAGE_URL,
} from "./StartPageAbout.sys.mts";

/** Launch-local dev port resolver (FDH Gate 2 task 2-B): dev/test URLs of
 * this launch resolve to the ports actually in use, not the base table. */
const launchDevPort = launchDevPorts();

// Floorp's mode-aware startup opens the versioned release notes. Firefox's
// separate milestone-update page ignores that choice and can open a second tab.
// Set the default before command-line startup; preserve explicit user overrides.
Services.prefs.getDefaultBranch("").setStringPref(
  "startup.homepage_override_url",
  "",
);

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

const { NoranekoConstants } = ChromeUtils.importESModule(
  "resource://noraneko/modules/NoranekoConstants.sys.mjs",
);

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

function installFloorpIPProtectionUIEarly(): boolean {
  try {
    const { FloorpIPProtectionUI } = ChromeUtils.importESModule(
      "resource://noraneko/modules/ipprotection/FloorpIPProtectionUI.sys.mjs",
    );
    return FloorpIPProtectionUI.installEarly();
  } catch (error) {
    console.error(
      "[FloorpIPProtectionUI] Failed to install the early runtime adapter:",
      error,
    );
    return false;
  }
}

const isFloorpIPProtectionUIReady = installFloorpIPProtectionUIEarly();

export const env = Services.env;
export const isMainBrowser = env.get("MOZ_BROWSER_TOOLBOX_PORT") === "";

export let isFirstRun = false;
export let isUpdated = false;

const executedFunctions = new Set<string>();
const RELEASE_NOTES_URL =
  `https://blog.floorp.app/release/${NoranekoConstants.version2}`;

export function executeOnce(id: string, callback: () => void): boolean {
  if (executedFunctions.has(id)) {
    return false;
  }

  callback();

  executedFunctions.add(id);
  return true;
}

function initializeVersionInfo(): void {
  isFirstRun = !Services.prefs.getStringPref(
    "browser.startup.homepage_override.mstone",
    undefined,
  );

  const nowVersion = AppConstants.MOZ_APP_VERSION_DISPLAY;
  const oldVersionPref = Services.prefs.getStringPref(
    "floorp.startup.oldVersion",
    undefined,
  );

  if (oldVersionPref !== nowVersion && !isFirstRun) {
    isUpdated = true;
  }

  if (!Services.prefs.prefHasUserValue(RELEASE_NOTES_PREFS.audience)) {
    Services.prefs.setStringPref(
      RELEASE_NOTES_PREFS.audience,
      releaseNotesAudience(
        isFirstRun,
        oldVersionPref,
        Services.prefs.getBoolPref("floorp.browser.welcome.page.shown", false),
      ),
    );
  }

  Services.prefs.setStringPref("floorp.startup.oldVersion", nowVersion);
}

export function onFinalUIStartup(): void {
  Services.obs.removeObserver(onFinalUIStartup, "final-ui-startup");

  createDefaultUserChromeFiles().catch((error) => {
    console.error("Failed to create default userChrome files:", error);
  });

  openReleaseNotesInRecentWindow()
    .catch(console.error)
    .then(showReleaseNotesChoice)
    .catch(console.error);

  // int OS Modules
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/OSGlue.sys.mjs",
  );
  // Localhost OS server (self-controlled by prefs)
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-server/server.sys.mjs",
  );
  // init i18n
  ChromeUtils.importESModule(
    "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
  );
}

async function openReleaseNotesInRecentWindow(): Promise<void> {
  const { SessionStore } = await ChromeUtils.importESModule(
    "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
  );

  await SessionStore.promiseInitialized;

  if (
    !isUpdated ||
    effectiveReleaseNotesMode(
        Services.prefs.getStringPref(RELEASE_NOTES_PREFS.mode, "blocking"),
        Services.prefs.getBoolPref(RELEASE_NOTES_PREFS.confirmed, false),
      ) ===
      "disabled"
  ) {
    return;
  }

  const recentWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as Window | null;

  if (!recentWindow) {
    console.warn("[NoranekoStartup] No recent window found");
    return;
  }

  const tabBrowser = recentWindow.gBrowser;
  const newTab = tabBrowser.addTab(RELEASE_NOTES_URL, {
    relatedToCurrent: false,
    inBackground: false,
    skipAnimation: false,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });

  const WORKSPACE_TAB_ATTRIBUTION_ID = "floorpWorkspaceId";
  let currentWorkspaceID: string | null = null;

  // Poll for workspacesFuncs if not immediately available or if ID is missing.
  // We'll give it a few tries (e.g. up to 2 seconds) to let WorkspacesService initialize.
  const waitForWorkspaceID = async (): Promise<string | null> => {
    let attempts = 0;
    while (attempts < 20) {
      const funcs = (recentWindow as Window).workspacesFuncs;
      if (funcs?.getSelectedWorkspaceID) {
        try {
          const id = funcs.getSelectedWorkspaceID();
          if (id) return id;
        } catch (e) {
          console.error("[NoranekoStartup] Failed to get workspace ID", e);
        }
      }
      await new Promise((r) => setTimeout(r, 100));
      attempts++;
    }
    return null;
  };

  currentWorkspaceID = await waitForWorkspaceID();

  if (currentWorkspaceID) {
    try {
      newTab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, currentWorkspaceID);
    } catch (e) {
      console.error(
        "[NoranekoStartup] Failed to set workspace for release notes tab",
        e,
      );
    }
  } else {
    console.warn(
      "[NoranekoStartup] Workspaces service not ready or no ID found, tab may open in default workspace.",
    );
  }

  tabBrowser.selectedTab = newTab;
}

function showReleaseNotesChoice(): void {
  if (
    !shouldShowReleaseNotesChoice(
      Services.prefs.getStringPref(RELEASE_NOTES_PREFS.audience, "existing"),
      Services.prefs.getBoolPref(RELEASE_NOTES_PREFS.confirmed, false),
      Services.prefs.getBoolPref(RELEASE_NOTES_PREFS.promptShown, false),
    )
  ) {
    return;
  }

  const recentWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as Window | null;
  if (!recentWindow) return;
  const tab = recentWindow.gBrowser.addTab("about:welcome?releaseNotes=1", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  const workspaceID = recentWindow.workspacesFuncs?.getSelectedWorkspaceID?.();
  if (workspaceID) tab.setAttribute("floorpWorkspaceId", workspaceID);
  recentWindow.gBrowser.selectedTab = tab;
  Services.prefs.setBoolPref(RELEASE_NOTES_PREFS.promptShown, true);
}

async function createDefaultUserChromeFiles(): Promise<void> {
  const chromeDir = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome",
  );
  const chromeExists = await IOUtils.exists(chromeDir);

  if (!chromeExists) {
    const userChromeCssPath = PathUtils.join(chromeDir, "userChrome.css");
    const userContentCssPath = PathUtils.join(chromeDir, "userContent.css");

    await IOUtils.writeUTF8(
      userChromeCssPath,
      `
/*************************************************************************************************************************************************************************************************************************************************************

"userChrome.css" is a custom CSS file that can be used to specify CSS style rules for Floorp's interface (NOT internal site) using "chrome" privileges.
For instance, if you want to hide the tab bar, you can use the following CSS rule:

**************************************
#TabsToolbar {                       *
    display: none !important;        *
}                                    *
**************************************

NOTE: You can use the userChrome.css file without change preferences (about:config)

Quote: https://userChrome.org | https://github.com/topics/userchrome 

************************************************************************************************************************************************************************************************************************************************************/

@charset "UTF-8";
@-moz-document url(chrome://browser/content/browser.xhtml) {
/* Please write your custom CSS under this line*/


}
`,
    );

    await IOUtils.writeUTF8(
      userContentCssPath,
      `
/*************************************************************************************************************************************************************************************************************************************************************

"userContent.css" is a custom CSS file that can be used to specify CSS style rules for Floorp's internal site using "chrome" privileges.
For instance, if you want to apply CSS at "about:newtab" and "about:home", you can use the following CSS rule:

***********************************************************************
@-moz-document url-prefix("about:newtab"), url-prefix("about:home") { *
                                                                      *
* Write your css *                                                    *
                                                                      *
}                                                                     *
***********************************************************************

NOTE: You can use the userContent.css file without change preferences (about:config)

************************************************************************************************************************************************************************************************************************************************************/

@charset "UTF-8";
/* Please write your custom CSS under this line*/
`,
    );
  }
}

async function isResourceAvailable(url: string): Promise<boolean> {
  try {
    const response = await fetch(url);
    return response.ok;
  } catch (error) {
    console.error(`[noraneko] Failed to check resource: ${url}`, error);
    return false;
  }
}

// Only for dev build
async function setupNoranekoNewTab(): Promise<void> {
  const { AboutNewTab } = ChromeUtils.importESModule(
    "resource:///modules/AboutNewTab.sys.mjs",
  );

  if (
    (await isResourceAvailable(
      "chrome://noraneko-newtab/content/index.html",
    )) === false
  ) {
    // Fallback for dev build about:newtab if file doesn't exist
    AboutNewTab.newTabURL = launchDevPort.httpOrigin("newtab", "/");
  }
}

async function checkNewtabUserPreference(): Promise<boolean> {
  if (
    (await isResourceAvailable(
      "chrome://noraneko-newtab/content/index.html",
    )) === false
  ) {
    // If chrome resource is missing, we might be in dev mode with localhost
    // For specific check, we assume false here unless we want to check localhost too.
    // However, the original code returned false if file check failed.
    return false;
  }

  const result = Services.prefs.getStringPref(
    "floorp.design.configs",
    undefined,
  );

  if (!result) {
    return true;
  }

  const data = JSON.parse(result);

  return !data.uiCustomization.disableFloorpStart;
}

/* Register Custom About Pages
 *
 * Credits: angelbruni/Geckium on GitHub
 * This code is the TypeScript version of the original JavaScript code.
 *
 * File referred: https://github.com/angelbruni/Geckium/blob/main/Profile%20Folder/chrome/JS/Geckium_aboutPageRegisterer.uc.js
 */

const getCustomAboutPages = async (): Promise<Record<string, string>> => {
  const customAboutPages: Record<string, string> = {
    hub: "chrome://noraneko-settings/content/index.html",
    welcome: "chrome://noraneko-welcome/content/index.html",
  };

  // Check and fallback for Dev Mode
  if (!(await isResourceAvailable(customAboutPages.hub))) {
    customAboutPages.hub = launchDevPort.httpOrigin("settings", "/");
  }
  if (!(await isResourceAvailable(customAboutPages.welcome))) {
    customAboutPages.welcome = launchDevPort.httpOrigin("welcome", "/");
  }

  if (await checkNewtabUserPreference()) {
    customAboutPages["newtab"] = START_PAGE_URL;
    customAboutPages["home"] = START_PAGE_URL;
    // Fallback for newtab in about pages map (though AboutNewTab handles the actual newtab url)
    if (!(await isResourceAvailable(customAboutPages["newtab"]))) {
      const localhostNewTab = launchDevPort.httpOrigin("newtab", "/");
      customAboutPages["newtab"] = localhostNewTab;
      customAboutPages["home"] = localhostNewTab;
    }
  }

  return customAboutPages;
};

async function registerCustomAboutPages(): Promise<void> {
  const customAboutPages = await getCustomAboutPages();
  // Newtab's resource mapping and Activity Stream share the native readiness signal.
  const newTabRedirector = customAboutPages.newtab
    ? Cc["@mozilla.org/network/protocol/about;1?what=newtab"]
      .getService(Ci.nsIAboutModule).wrappedJSObject
    : undefined;

  for (const aboutKey in customAboutPages) {
    registerCustomAboutPage(
      aboutKey,
      customAboutPages[aboutKey],
      aboutKey === "newtab" ? newTabRedirector : undefined,
    );
  }
  if (customAboutPages.newtab === START_PAGE_URL) {
    loadStartPageInPrivilegedAboutProcesses();
  }
}

async function setupBrowserOSComponents(): Promise<void> {
  await ChromeUtils.importESModule(
    "resource://noraneko/modules/os-automotor/OSAutomotor-manager.sys.mjs",
  );
}

function registerSsbCommandLineHandler(): void {
  executeOnce("register-ssb-command-line-handler", () => {
    const { SSBCommandLineHandler } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs",
    );
    const contractId = "@noraneko.org/commandlinehandler/general-start-ssb;1";
    const factory: nsIFactory = {
      createInstance<T extends nsIID>(iid: T): nsQIResult<T> {
        return new SSBCommandLineHandler().QueryInterface(iid);
      },
    };

    const registrar = getComponentRegistrar();
    if (!registrar) {
      console.error(
        "[NoranekoStartup] Failed to get nsIComponentRegistrar for SSB handler",
      );
      return;
    }
    registrar.registerFactory(
      Services.uuid.generateUUID(),
      "Floorp SSB command line handler",
      contractId,
      factory,
    );
    Services.catMan.addCategoryEntry(
      "command-line-handler",
      "m-floorp-ssb",
      contractId,
      false,
      true,
    );
  });
}

async function initializeExperiments() {
  const { Experiments } = await ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  );
  await Experiments.init();

  const { FloorpIPProtectionGate } = await ChromeUtils.importESModule(
    "resource://noraneko/modules/ipprotection/FloorpIPProtectionGate.sys.mjs",
  );
  FloorpIPProtectionGate.apply(isFloorpIPProtectionUIReady);
}

(async () => {
  registerSsbCommandLineHandler();
  // macOS Web App ownership has a separate native lifecycle; do not replay
  // those windows until restart has been validated with the native shim.
  if (isMainBrowser && ["WINNT", "Linux"].includes(Services.appinfo.OS)) {
    try {
      const { SsbRestart } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/SsbRestart.sys.mjs",
      );
      SsbRestart.init();
    } catch (error) {
      console.error("[NoranekoStartup] PWA restart setup failed:", error);
    }
  }
  await registerCustomAboutPages();
  initializeVersionInfo();
  await initializeExperiments();
  await setupNoranekoNewTab();
  await setupBrowserOSComponents();
})().catch(console.error);

if (isMainBrowser) {
  Services.obs.addObserver(onFinalUIStartup, "final-ui-startup");
}
