/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
import type { Version2UpdateStatus } from "./NoranekoUpdateChecker.sys.mts";

const { NoranekoConstants } = ChromeUtils.importESModule(
  "resource://noraneko/modules/NoranekoConstants.sys.mjs",
);

const { checkForUpdates, saveCurrentVersion, triggerUpdateIfNeeded } =
  ChromeUtils.importESModule(
    "resource://noraneko/modules/NoranekoUpdateChecker.sys.mjs",
  );

// Update check interval: 24 hours in milliseconds
const UPDATE_CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000;

// Store interval ID for potential cleanup (exported for testing/shutdown)
export let _updateCheckIntervalId: ReturnType<typeof setInterval> | null = null;

type UpdateCheckResult = { triggered: boolean; status: Version2UpdateStatus };

export const env = Services.env;
export const isMainBrowser = env.get("MOZ_BROWSER_TOOLBOX_PORT") === "";

export let isFirstRun = false;
export let isUpdated = false;

const executedFunctions = new Set<string>();
const RELEASE_NOTES_URL = `https://blog.floorp.app/release/${NoranekoConstants.version2}`;

export function executeOnce(id: string, callback: () => void): boolean {
  if (executedFunctions.has(id)) {
    return false;
  }

  callback();

  executedFunctions.add(id);
  return true;
}

function initializeVersionInfo(): void {
  const updateInfo = checkForUpdates();
  isFirstRun = updateInfo.isFirstRun;
  isUpdated = updateInfo.isUpdated;
  saveCurrentVersion();
}

export function onFinalUIStartup(): void {
  Services.obs.removeObserver(onFinalUIStartup, "final-ui-startup");

  createDefaultUserChromeFiles().catch((error) => {
    console.error("Failed to create default userChrome files:", error);
  });

  openReleaseNotesInRecentWindow();

  // Start version2 update checker (startup + periodic)
  startVersion2UpdateChecker();

  // init OS Modules
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

/**
 * Start version2 update checker.
 * Checks immediately at startup and then periodically.
 */
function startVersion2UpdateChecker(): void {
  console.log("[NoranekoStartup] Initializing version2 update checker...");
  console.log(
    `[NoranekoStartup] Local version2: ${NoranekoConstants.version2}`,
  );
  console.log(
    `[NoranekoStartup] Check interval: ${UPDATE_CHECK_INTERVAL_MS}ms (${UPDATE_CHECK_INTERVAL_MS / 1000 / 60 / 60}h)`,
  );

  // Check immediately at startup (with a small delay to not block UI)
  ChromeUtils.idleDispatch(() => {
    console.log("[NoranekoStartup] Starting startup update check...");
    triggerUpdateIfNeeded()
      .then((result: UpdateCheckResult) => {
        console.log("[NoranekoStartup] Startup update check completed:");
        console.log(`  - hasUpdate: ${result.status.hasUpdate}`);
        console.log(`  - triggered: ${result.triggered}`);
        console.log(`  - localVersion2: ${result.status.localVersion2}`);
        console.log(`  - remoteVersion2: ${result.status.remoteVersion2}`);
        console.log(`  - updateType: ${result.status.updateType}`);
      })
      .catch((error: unknown) => {
        console.error("[NoranekoStartup] Startup update check failed:", error);
      });
  });

  // Set up periodic check
  _updateCheckIntervalId = setInterval(() => {
    console.log("[NoranekoStartup] Starting periodic update check...");
    triggerUpdateIfNeeded()
      .then((result: UpdateCheckResult) => {
        console.log("[NoranekoStartup] Periodic update check completed:");
        console.log(`  - hasUpdate: ${result.status.hasUpdate}`);
        console.log(`  - triggered: ${result.triggered}`);
      })
      .catch((error: unknown) => {
        console.error("[NoranekoStartup] Periodic update check failed:", error);
      });
  }, UPDATE_CHECK_INTERVAL_MS);

  // Add observer for cleanup on shutdown
  Services.obs.addObserver(() => {
    if (_updateCheckIntervalId !== null) {
      clearInterval(_updateCheckIntervalId);
      _updateCheckIntervalId = null;
      console.log("[NoranekoStartup] Update checker timer cleared");
    }
  }, "quit-application");

  console.log("[NoranekoStartup] Version2 update checker started");
}

async function openReleaseNotesInRecentWindow(): Promise<void> {
  const { SessionStore } = await ChromeUtils.importESModule(
    "resource:///modules/sessionstore/SessionStore.sys.mjs",
  );

  await SessionStore.promiseInitialized;

  if (!isUpdated) {
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

  tabBrowser.addTab("about:welcome?upgrade=12", {
    relatedToCurrent: false,
    inBackground: true,
    skipAnimation: false,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });

  recentWindow.addEventListener(
    "DOMContentLoaded",
    () => {
      tabBrowser.selectedTab = newTab;
    },
    { once: true },
  );
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

// Only for dev build
async function setupNoranekoNewTab(): Promise<void> {
  const { AboutNewTab } = ChromeUtils.importESModule(
    "resource:///modules/AboutNewTab.sys.mjs",
  );

  if ((await isNewTabFileAvailable()) === false) {
    // Fallback for dev build about:newtab if file doesn't exist
    AboutNewTab.newTabURL = "http://localhost:5186/";
  }
}

async function isNewTabFileAvailable(): Promise<boolean> {
  try {
    const response = await fetch("chrome://noraneko-newtab/content/index.html");
    return response.ok;
  } catch (error) {
    console.error("[noraneko] Failed to check newtab file:", error);
    return false;
  }
}

async function checkNewtabUserPreference(): Promise<boolean> {
  if ((await isNewTabFileAvailable()) === false) {
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

  if (await checkNewtabUserPreference()) {
    customAboutPages["newtab"] = "chrome://noraneko-newtab/content/index.html";
    customAboutPages["home"] = "chrome://noraneko-newtab/content/index.html";
  }

  return customAboutPages;
};

class CustomAboutPage {
  private _uri: nsIURI;

  constructor(urlString: string) {
    this._uri = Services.io.newURI(urlString);
  }

  get uri(): nsIURI {
    return this._uri;
  }

  newChannel(_uri: nsIURI, loadInfo: nsILoadInfo): nsIChannel {
    const new_ch = Services.io.newChannelFromURIWithLoadInfo(
      this.uri,
      loadInfo,
    );
    new_ch.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return new_ch;
  }

  getURIFlags(_uri: nsIURI): number {
    if (!this.uri) {
      throw new Error("URI is not defined");
    }
    return Ci.nsIAboutModule.ALLOW_SCRIPT | Ci.nsIAboutModule.IS_SECURE_CHROME_UI;
  }

  getChromeURI(_uri: nsIURI): nsIURI {
    return this.uri;
  }

  QueryInterface = ChromeUtils.generateQI(["nsIAboutModule"]);
}

async function registerCustomAboutPages(): Promise<void> {
  const customAboutPages = await getCustomAboutPages();

  for (const aboutKey in customAboutPages) {
    const AboutModuleFactory: nsIFactory = {
      createInstance<T extends nsIID>(iid: T): nsQIResult<T> {
        return new CustomAboutPage(customAboutPages[aboutKey]).QueryInterface(
          iid,
        );
      },
    };

    let registrar: nsIComponentRegistrar | null = null;
    const cm = Components.manager;
    if (cm !== undefined && cm !== null) {
      registrar = cm as unknown as nsIComponentRegistrar;
      // Some environments require explicit QI; do it when available.
      try {
        const maybeQI = (
          cm as unknown as { QueryInterface?: (iid: nsIID) => unknown }
        ).QueryInterface;
        if (typeof maybeQI === "function") {
          // @ts-ignore: Gecko Components.manager supports QueryInterface at runtime
          registrar = cm.QueryInterface(Ci.nsIComponentRegistrar);
        }
      } catch (e) {
        console.error("Failed to get nsIComponentRegistrar:", e);
      }
    }
    if (!registrar) {
      console.error("Failed to get nsIComponentRegistrar");
      continue;
    }
    registrar.registerFactory(
      Services.uuid.generateUUID(),
      `about:${aboutKey}`,
      `@mozilla.org/network/protocol/about;1?what=${aboutKey}`,
      AboutModuleFactory,
    );
  }
}

async function setupBrowserOSComponents(): Promise<void> {
  await ChromeUtils.importESModule(
    "resource://noraneko/modules/os-automotor/OSAutomotor-manager.sys.mjs",
  );
}

async function initializeExperiments() {
  const { Experiments } = await ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  );
  await Experiments.init();
}

(async () => {
  await registerCustomAboutPages();
  initializeVersionInfo();
  await initializeExperiments();
  await setupNoranekoNewTab();
  await setupBrowserOSComponents();
})().catch(console.error);

if (isMainBrowser) {
  Services.obs.addObserver(onFinalUIStartup, "final-ui-startup");
}
