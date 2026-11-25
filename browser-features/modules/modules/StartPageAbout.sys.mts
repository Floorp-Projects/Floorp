/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export const START_PAGE_URL = "chrome://noraneko-newtab/content/index.html";

const PRIVILEGED_ABOUT_REMOTE_TYPE = "privilegedabout";
const START_PAGE_ABOUT_KEYS = ["newtab", "home"];
const START_PAGE_PROCESS_SCRIPT =
  'data:,ChromeUtils.importESModule("resource://noraneko/modules/StartPageAbout.sys.mjs").registerStartPageInContentProcess();';

/**
 * Get nsIComponentRegistrar from Components.manager via QueryInterface.
 * Components.manager needs explicit QI to access registerFactory.
 */
export function getComponentRegistrar(): nsIComponentRegistrar | null {
  try {
    const cm = Components.manager;
    if (cm === undefined || cm === null) {
      return null;
    }
    // Components.manager has QueryInterface at runtime but TypeScript defs don't reflect it
    const maybeQI =
      (cm as unknown as { QueryInterface?: (iid: nsIID) => unknown })
        .QueryInterface;
    if (typeof maybeQI !== "function") {
      return null;
    }
    return maybeQI.call(cm, Ci.nsIComponentRegistrar) as nsIComponentRegistrar;
  } catch (e) {
    console.error("[StartPageAbout] Failed to get nsIComponentRegistrar:", e);
    return null;
  }
}

export function isStartPageAboutPage(aboutKey: string, url: string): boolean {
  return START_PAGE_ABOUT_KEYS.includes(aboutKey) && url === START_PAGE_URL;
}

export class CustomAboutPage {
  private _uri: nsIURI;
  private _loadInContentProcess: boolean;

  constructor(urlString: string, loadInContentProcess = false) {
    this._uri = Services.io.newURI(urlString);
    this._loadInContentProcess = loadInContentProcess;
  }

  get uri(): nsIURI {
    return this._uri;
  }

  newChannel(_uri: nsIURI, loadInfo: nsILoadInfo): nsIChannel {
    const query = _uri.query ? `?${_uri.query}` : "";
    const ref = _uri.ref ? `#${_uri.ref}` : "";
    const targetUri = Services.io.newURI(`${this.uri.spec}${query}${ref}`);
    const new_ch = Services.io.newChannelFromURIWithLoadInfo(
      targetUri,
      loadInfo,
    );
    if (this._loadInContentProcess) {
      new_ch.originalURI = _uri;
    } else {
      new_ch.owner = Services.scriptSecurityManager.getSystemPrincipal();
    }
    return new_ch;
  }

  getURIFlags(_uri: nsIURI): number {
    if (!this.uri) {
      throw new Error("URI is not defined");
    }

    if (this._loadInContentProcess) {
      return (
        (Ci.nsIAboutModule.ALLOW_SCRIPT as number) |
        (Ci.nsIAboutModule.ENABLE_INDEXED_DB as number) |
        (Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD as number) |
        (Ci.nsIAboutModule.URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS as number) |
        (Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT as number)
      );
    }

    return (
      (Ci.nsIAboutModule.ALLOW_SCRIPT as number) |
      (Ci.nsIAboutModule.IS_SECURE_CHROME_UI as number)
    );
  }

  getChromeURI(_uri: nsIURI): nsIURI {
    return this.uri;
  }

  QueryInterface = ChromeUtils.generateQI(["nsIAboutModule"]);
}

/**
 * Registers `about:<aboutKey>` for `url`. `wrappedJSObject` keeps callers of
 * the replaced Firefox module (e.g. about:newtab's redirector) working.
 */
export function registerCustomAboutPage(
  aboutKey: string,
  url: string,
  wrappedJSObject?: unknown,
): void {
  const loadInContentProcess = isStartPageAboutPage(aboutKey, url);
  const factory: nsIFactory = {
    createInstance<T extends nsIID>(iid: T): nsQIResult<T> {
      const page = new CustomAboutPage(url, loadInContentProcess);
      if (wrappedJSObject !== undefined) {
        Object.defineProperty(page, "wrappedJSObject", {
          value: wrappedJSObject,
        });
      }
      return page.QueryInterface(iid);
    },
  };

  const registrar = getComponentRegistrar();
  if (!registrar) {
    console.error("[StartPageAbout] Failed to get nsIComponentRegistrar");
    return;
  }
  registrar.registerFactory(
    Services.uuid.generateUUID(),
    `about:${aboutKey}`,
    `@mozilla.org/network/protocol/about;1?what=${aboutKey}`,
    factory,
  );
}

function getNewTabRedirector(): unknown {
  return Cc["@mozilla.org/network/protocol/about;1?what=newtab"]
    .getService(Ci.nsIAboutModule).wrappedJSObject;
}

/**
 * Floorp Start loads in the privileged about content process, where Firefox
 * creates the about:newtab and about:home channels with its own about module.
 */
export function registerStartPageInContentProcess(): void {
  if (Services.appinfo.remoteType !== PRIVILEGED_ABOUT_REMOTE_TYPE) {
    return;
  }
  const newTabRedirector = getNewTabRedirector();
  for (const aboutKey of START_PAGE_ABOUT_KEYS) {
    registerCustomAboutPage(
      aboutKey,
      START_PAGE_URL,
      aboutKey === "newtab" ? newTabRedirector : undefined,
    );
  }
}

const startPageProcesses = new WeakSet<ProcessMessageManager>();

function loadIntoPrivilegedAboutProcesses(): void {
  for (let i = 0; i < Services.ppmm.childCount; i++) {
    const mm = Services.ppmm.getChildAt(i) as ProcessMessageManager | null;
    if (
      !mm || mm.isInProcess ||
      mm.remoteType !== PRIVILEGED_ABOUT_REMOTE_TYPE ||
      startPageProcesses.has(mm)
    ) {
      continue;
    }
    startPageProcesses.add(mm);
    mm.loadProcessScript(START_PAGE_PROCESS_SCRIPT, false);
  }
}

export function loadStartPageInPrivilegedAboutProcesses(): void {
  loadIntoPrivilegedAboutProcesses();
  Services.obs.addObserver(
    loadIntoPrivilegedAboutProcesses,
    "ipc:content-created",
  );
  Services.obs.addObserver(
    loadIntoPrivilegedAboutProcesses,
    "process-type-set",
  );
}
