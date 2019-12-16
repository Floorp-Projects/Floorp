/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A Site Specific Browser intends to allow the user to navigate through the
 * chosen site in the SSB UI. Any attempt to load something outside the site
 * should be loaded in a normal browser. In order to achieve this we have to use
 * various APIs to listen for attempts to load new content and take appropriate
 * action. Often this requires returning synchronous responses to method calls
 * in content processes and will require data about the SSB in order to respond
 * correctly. Here we implement an architecture to support that:
 *
 * In the main process the SiteSpecificBrowser class implements all the
 * functionality involved with managing an SSB. All content processes can
 * synchronously retrieve a matching SiteSpecificBrowserBase that has enough
 * data about the SSB in order to be able to respond to load requests
 * synchronously. To support this we give every SSB a unique ID (UUID based)
 * and the appropriate data is shared via sharedData. Once created the ID can be
 * used to retrieve the SiteSpecificBrowser instance in the main process or
 * SiteSpecificBrowserBase instance in any content process.
 */

var EXPORTED_SYMBOLS = [
  "SiteSpecificBrowserService",
  "SiteSpecificBrowserBase",
  "SiteSpecificBrowser",
  "SSBCommandLineHandler",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ManifestObtainer: "resource://gre/modules/ManifestObtainer.jsm",
  ManifestProcessor: "resource://gre/modules/ManifestProcessor.jsm",
  KeyValueService: "resource://gre/modules/kvstore.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

/**
 * A schema version for the SSB data stored in the kvstore.
 *
 * Version 1 has the `manifest` and `config` properties.
 */
const DATA_VERSION = 1;

/**
 * The prefix used for SSB ids in the store.
 */
const SSB_STORE_PREFIX = "ssb:";

/**
 * A prefix that will sort immediately after any SSB ids in the store.
 */
const SSB_STORE_LAST = "ssb;";

function uuid() {
  return Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator)
    .generateUUID()
    .toString();
}

const sharedDataKey = id => `SiteSpecificBrowserBase:${id}`;
const storeKey = id => SSB_STORE_PREFIX + id;

const IS_MAIN_PROCESS =
  Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT;

/**
 * Tests whether an app manifest's scope includes the given URI.
 *
 * @param {nsIURI} scope the manifest's scope.
 * @param {nsIURI} uri the URI to test.
 * @returns {boolean} true if the uri is included in the scope.
 */
function scopeIncludes(scope, uri) {
  // https://w3c.github.io/manifest/#dfn-within-scope
  if (scope.prePath != uri.prePath) {
    return false;
  }

  return uri.filePath.startsWith(scope.filePath);
}

/**
 * Generates a basic app manifest for a URI.
 *
 * @param {nsIURI} uri the start URI for the site.
 * @return {Manifest} an app manifest.
 */
function manifestForURI(uri) {
  try {
    let manifestURI = Services.io.newURI("/manifest.json", null, uri);
    return ManifestProcessor.process({
      jsonText: "{}",
      manifestURL: manifestURI.spec,
      docURL: uri.spec,
    });
  } catch (e) {
    console.error(`Failed to generate a SSB manifest for ${uri.spec}.`, e);
    throw e;
  }
}

/**
 * Generates an app manifest for a site loaded in a browser element.
 *
 * @param {Element} browser the browser element the site is loaded in.
 * @return {Promise<Manifest>} an app manifest.
 */
async function buildManifestForBrowser(browser) {
  let manifest = null;
  try {
    manifest = await ManifestObtainer.browserObtainManifest(browser);
  } catch (e) {
    // We can function without a valid manifest.
    console.error(e);
  }

  // Reject the manifest if its scope doesn't include the current document.
  if (
    !manifest ||
    !scopeIncludes(Services.io.newURI(manifest.scope), browser.currentURI)
  ) {
    manifest = manifestForURI(browser.currentURI);
  }

  // Cache all the icons as data URIs since we can need access to them when
  // the website is not loaded.
  manifest.icons = (await Promise.all(
    manifest.icons.map(async icon => {
      if (icon.src.startsWith("data:")) {
        return icon;
      }

      let actor = browser.browsingContext.currentWindowGlobal.getActor(
        "SiteSpecificBrowser"
      );
      try {
        icon.src = await actor.sendQuery("LoadIcon", icon.src);
      } catch (e) {
        // Bad icon, drop it from the list.
        return null;
      }

      return icon;
    })
  )).filter(icon => icon);

  return manifest;
}

/**
 * Maintains an ID -> SSB mapping in the main process. Content processes should
 * use sharedData to get a SiteSpecificBrowserBase.
 *
 * We do not currently expire data from here so once created an SSB instance
 * lives for the lifetime of the application. The expectation is that the
 * numbers of different SSBs used will be low and the memory use will also
 * be low.
 */
const SSBMap = new Map();

/**
 * The base contains the data about an SSB instance needed in content processes.
 *
 * The only data needed currently is site's `scope` which is just a URI.
 */
class SiteSpecificBrowserBase {
  /**
   * Creates a new SiteSpecificBrowserBase. Generally should only be called by
   * code within this module.
   *
   * @param {nsIURI} scope the scope for the SSB.
   */
  constructor(scope) {
    this._scope = scope;
  }

  /**
   * Gets the SiteSpecifcBrowserBase for an ID. If this is the main process this
   * will instead return the SiteSpecificBrowser instance itself but generally
   * don't call this from the main process.
   *
   * The returned object is not "live" and will not be updated with any
   * configuration changes from the main process so do not cache this, get it
   * when needed and then discard.
   *
   * @param {string} id the SSB ID.
   * @return {SiteSpecificBrowserBase|null} the instance if it exists.
   */
  static get(id) {
    if (IS_MAIN_PROCESS) {
      return SiteSpecificBrowser.get(id);
    }

    let key = sharedDataKey(id);
    if (!Services.cpmm.sharedData.has(key)) {
      return null;
    }

    let scope = Services.io.newURI(Services.cpmm.sharedData.get(key));
    return new SiteSpecificBrowserBase(scope);
  }

  /**
   * Checks whether the given URI is considered to be a part of this SSB or not.
   * Any URIs that return false should be loaded in a normal browser.
   *
   * @param {nsIURI} uri the URI to check.
   * @return {boolean} whether this SSB can load the URI.
   */
  canLoad(uri) {
    // Always allow loading about:blank as it is the initial page for iframes.
    if (uri.spec == "about:blank") {
      return true;
    }

    return scopeIncludes(this._scope, uri);
  }
}

/**
 * The SSB instance used in the main process.
 *
 * We maintain three pieces of data for an SSB:
 *
 * First is the string UUID for identification purposes.
 *
 * Second is an app manifest (https://w3c.github.io/manifest/). If the site does
 * not provide one a basic one will be automatically generated. The intent is to
 * never modify this such that it can be updated from the site when needed
 * without blowing away any configuration changes a user might want to make to
 * the SSB itself.
 *
 * Thirdly there is the SSB configuration. This includes internal data, user
 * overrides for the app manifest and custom SSB extensions to the app manifest.
 *
 * We pass data based on these down to the SiteSpecificBrowserBase in this and
 * other processes (via `_updateSharedData`).
 */
class SiteSpecificBrowser extends SiteSpecificBrowserBase {
  /**
   * Creates a new SiteSpecificBrowser. Generally should only be called by
   * code within this module.
   *
   * @param {string} id the SSB's unique ID.
   * @param {Manifest} manifest the app manifest for the SSB.
   * @param {object?} config the SSB configuration settings.
   */
  constructor(id, manifest, config = {}) {
    if (!IS_MAIN_PROCESS) {
      throw new Error(
        "SiteSpecificBrowser instances are only available in the main process."
      );
    }

    super(Services.io.newURI(manifest.scope));
    this._id = id;
    this._manifest = manifest;
    this._config = Object.assign(
      {
        needsUpdate: true,
        persisted: false,
      },
      config
    );

    // Cache the SSB for retrieval.
    SSBMap.set(id, this);

    this._updateSharedData();
  }

  /**
   * Loads the SiteSpecificBrowser for the given ID.
   *
   * @param {string} id the SSB's unique ID.
   * @param {object?} data the data to deserialize from. Do not use externally.
   * @return {Promise<SiteSpecificBrowser?>} the instance if it exists.
   */
  static async load(id, data = null) {
    if (!IS_MAIN_PROCESS) {
      throw new Error(
        "SiteSpecificBrowser instances are only available in the main process."
      );
    }

    if (SSBMap.has(id)) {
      return SSBMap.get(id);
    }

    if (!data) {
      let kvstore = await SiteSpecificBrowserService.getKVStore();
      data = await kvstore.get(storeKey(id), null);
    }

    if (!data) {
      return null;
    }

    try {
      let parsed = JSON.parse(data);
      parsed.config.persisted = true;
      return new SiteSpecificBrowser(id, parsed.manifest, parsed.config);
    } catch (e) {
      console.error(e);
    }

    return null;
  }

  /**
   * Gets the SiteSpecifcBrowser for an ID. Can only be called from the main
   * process.
   *
   * @param {string} id the SSB ID.
   * @return {SiteSpecificBrowser|null} the instance if it exists.
   */
  static get(id) {
    if (!IS_MAIN_PROCESS) {
      throw new Error(
        "SiteSpecificBrowser instances are only available in the main process."
      );
    }

    return SSBMap.get(id);
  }

  /**
   * Creates an SSB from a parsed app manifest.
   *
   * @param {Manifest} manifest the app manifest for the site.
   * @return {Promise<SiteSpecificBrowser>} the generated SSB.
   */
  static async createFromManifest(manifest) {
    if (!SiteSpecificBrowserService.isEnabled) {
      throw new Error("Site specific browsing is disabled.");
    }

    if (!manifest.scope.startsWith("https:")) {
      throw new Error(
        "Site specific browsers can only be opened for secure sites."
      );
    }

    return new SiteSpecificBrowser(uuid(), manifest, { needsUpdate: false });
  }

  /**
   * Creates an SSB from a site loaded in a browser element.
   *
   * @param {Element} browser the browser element the site is loaded in.
   * @return {Promise<SiteSpecificBrowser>} the generated SSB.
   */
  static async createFromBrowser(browser) {
    if (!SiteSpecificBrowserService.isEnabled) {
      throw new Error("Site specific browsing is disabled.");
    }

    if (!browser.currentURI.schemeIs("https")) {
      throw new Error(
        "Site specific browsers can only be opened for secure sites."
      );
    }

    return SiteSpecificBrowser.createFromManifest(
      await buildManifestForBrowser(browser)
    );
  }

  /**
   * Creates an SSB from a sURI.
   *
   * @param {nsIURI} uri the uri to generate from.
   * @return {SiteSpecificBrowser} the generated SSB.
   */
  static createFromURI(uri) {
    if (!SiteSpecificBrowserService.isEnabled) {
      throw new Error("Site specific browsing is disabled.");
    }

    if (!uri.schemeIs("https")) {
      throw new Error(
        "Site specific browsers can only be opened for secure sites."
      );
    }

    return new SiteSpecificBrowser(uuid(), manifestForURI(uri));
  }

  /**
   * Caches the data needed by content processes.
   */
  _updateSharedData() {
    Services.ppmm.sharedData.set(sharedDataKey(this.id), this._scope.spec);
    Services.ppmm.sharedData.flush();
  }

  /**
   * Persists the data to the store if needed. When a change in configuration
   * has occured call this.
   */
  async _maybeSave() {
    // If this SSB is persisted then update it in the data store.
    if (this._config.persisted) {
      let data = {
        manifest: this._manifest,
        config: this._config,
      };

      let kvstore = await SiteSpecificBrowserService.getKVStore();
      await kvstore.put(storeKey(this.id), JSON.stringify(data));
    }
  }

  /**
   * Installs this SiteSpecificBrowser such that it exists for future instances
   * of the application and will appear in lists of installed SSBs.
   */
  async install() {
    if (this._config.persisted) {
      return;
    }

    this._config.persisted = true;
    await this._maybeSave();
  }

  /**
   * Uninstalls this SiteSpecificBrowser. Undoes eveerything above. The SSB is
   * still usable afterwards.
   */
  async uninstall() {
    if (!this._config.persisted) {
      return;
    }

    this._config.persisted = false;
    let kvstore = await SiteSpecificBrowserService.getKVStore();
    await kvstore.delete(storeKey(this.id));
  }

  /**
   * The SSB's ID.
   */
  get id() {
    return this._id;
  }

  /**
   * The default URI to load.
   */
  get startURI() {
    return Services.io.newURI(this._manifest.start_url);
  }

  /**
   * Whether this SSB needs to be checked for an updated manifest.
   */
  get needsUpdate() {
    return this._config.needsUpdate;
  }

  /**
   * Updates this SSB from a new app manifest.
   *
   * @param {Manifest} manifest the new app manifest.
   */
  async updateFromManifest(manifest) {
    this._manifest = manifest;
    this._scope = Services.io.newURI(this._manifest.scope);
    this._config.needsUpdate = false;

    this._updateSharedData();
    await this._maybeSave();
  }

  /**
   * Updates this SSB from the site loaded in the browser element.
   *
   * @param {Element} browser the browser element.
   */
  async updateFromBrowser(browser) {
    let manifest = await buildManifestForBrowser(browser);
    await this.updateFromManifest(manifest);
  }

  /**
   * Launches a SSB by opening the necessary UI.
   *
   * @param {nsIURI?} the initial URI to load. If not provided uses the default.
   */
  launch(uri = null) {
    let sa = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

    let idstr = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    idstr.data = this.id;
    sa.appendElement(idstr);

    if (uri) {
      let uristr = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      uristr.data = uri.spec;
      sa.appendElement(uristr);
    }

    Services.ww.openWindow(
      null,
      "chrome://browser/content/ssb/ssb.html",
      "_blank",
      "chrome,dialog=no,all",
      sa
    );
  }
}

/**
 * Loads the KV store for SSBs. Should always resolve with a store even if that
 * means wiping whatever is currently on disk because it was unreadable.
 */
async function loadKVStore() {
  let dir = OS.Path.join(OS.Constants.Path.profileDir, "ssb");

  /**
   * Creates an empty store. Called when we know there is an empty directory.
   */
  async function createStore() {
    await OS.File.makeDir(dir);
    let kvstore = await KeyValueService.getOrCreate(dir, "ssb");
    await kvstore.put(
      "_meta",
      JSON.stringify({
        version: DATA_VERSION,
      })
    );

    return kvstore;
  }

  // First see if anything exists.
  try {
    let info = await OS.File.stat(dir);

    if (!info.isDir) {
      await OS.File.remove(dir, { ignoreAbsent: true });
      return createStore();
    }
  } catch (e) {
    if (e.becauseNoSuchFile) {
      return createStore();
    }
    throw e;
  }

  // Something exists, try to load it.
  try {
    let kvstore = await KeyValueService.getOrCreate(dir, "ssb");

    let meta = await kvstore.get("_meta", null);
    if (meta) {
      let data = JSON.parse(meta);
      if (data.version == DATA_VERSION) {
        return kvstore;
      }
      console.error(`SSB store is an unexpected version ${data.version}`);
    } else {
      console.error("SSB store was missing meta data.");
    }

    // We don't know how to handle this database, re-initialize it.
    await kvstore.clear();
    await kvstore.put(
      "_meta",
      JSON.stringify({
        version: DATA_VERSION,
      })
    );

    return kvstore;
  } catch (e) {
    console.error(e);

    // Something is very wrong. Wipe all our data and start again.
    await OS.File.removeDir(dir);
    return createStore();
  }
}

const SiteSpecificBrowserService = {
  kvstore: null,

  /**
   * Returns a promise that resolves to the KV store for SSBs.
   */
  getKVStore() {
    if (!this.kvstore) {
      this.kvstore = loadKVStore();
    }

    return this.kvstore;
  },

  /**
   * Returns a promise that resolves to an array of all of the installed SSBs.
   */
  async list() {
    let kvstore = await this.getKVStore();
    let list = await kvstore.enumerate(SSB_STORE_PREFIX, SSB_STORE_LAST);
    return Promise.all(
      Array.from(list).map(({ key: id, value: data }) =>
        SiteSpecificBrowser.load(id.substring(SSB_STORE_PREFIX.length), data)
      )
    );
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  SiteSpecificBrowserService,
  "isEnabled",
  "browser.ssb.enabled",
  false
);

async function startSSB(id) {
  // Loading the SSB is async. Until that completes and launches we will
  // be without an open window and the platform will not continue startup
  // in that case. Flag that a window is coming.
  Services.startup.enterLastWindowClosingSurvivalArea();

  // Whatever happens we must exitLastWindowClosingSurvivalArea when done.
  try {
    let ssb = await SiteSpecificBrowser.load(id);
    if (ssb) {
      ssb.launch();
    } else {
      dump(`No SSB installed as ID ${id}\n`);
    }
  } finally {
    Services.startup.exitLastWindowClosingSurvivalArea();
  }
}

class SSBCommandLineHandler {
  /* nsICommandLineHandler */
  handle(cmdLine) {
    if (!SiteSpecificBrowserService.isEnabled) {
      return;
    }

    let site = cmdLine.handleFlagWithParam("ssb", false);
    if (site) {
      cmdLine.preventDefault = true;

      try {
        let fixupInfo = Services.uriFixup.getFixupURIInfo(
          site,
          Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
        );

        let uri = fixupInfo.preferredURI;
        if (!uri) {
          dump(`Unable to parse '${site}' as a URI.\n`);
          return;
        }

        if (fixupInfo.fixupChangedProtocol && uri.schemeIs("http")) {
          uri = uri
            .mutate()
            .setScheme("https")
            .finalize();
        }
        let ssb = SiteSpecificBrowser.createFromURI(uri);
        ssb.launch();
      } catch (e) {
        dump(`Unable to parse '${site}' as a URI: ${e}\n`);
      }

      return;
    }

    let id = cmdLine.handleFlagWithParam("start-ssb", false);
    if (id) {
      cmdLine.preventDefault = true;

      startSSB(id);
    }
  }

  get helpInfo() {
    return "  --ssb <uri>        Open a site specific browser for <uri>.\n";
  }
}

SSBCommandLineHandler.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsICommandLineHandler,
]);
