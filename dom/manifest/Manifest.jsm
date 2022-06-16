/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Manifest.jsm is the top level api for managing installed web applications
 * https://www.w3.org/TR/appmanifest/
 *
 * It is used to trigger the installation of a web application via .install()
 * and to access the manifest data (including icons).
 *
 * TODO:
 *  - Trigger appropriate app installed events
 */

"use strict";

const { ManifestObtainer } = ChromeUtils.import(
  "resource://gre/modules/ManifestObtainer.jsm"
);
const { ManifestIcons } = ChromeUtils.import(
  "resource://gre/modules/ManifestIcons.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);

/**
 * Generates an hash for the given string.
 *
 * @note The generated hash is returned in base64 form.  Mind the fact base64
 * is case-sensitive if you are going to reuse this code.
 */
function generateHash(aString) {
  const cryptoHash = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  cryptoHash.init(Ci.nsICryptoHash.MD5);
  const stringStream = Cc[
    "@mozilla.org/io/string-input-stream;1"
  ].createInstance(Ci.nsIStringInputStream);
  stringStream.data = aString;
  cryptoHash.updateFromStream(stringStream, -1);
  // base64 allows the '/' char, but we can't use it for filenames.
  return cryptoHash.finish(true).replace(/\//g, "-");
}

/**
 * Trims the query parameters from a url
 */
function stripQuery(url) {
  return url.split("?")[0];
}

// Folder in which we store the manifest files
const MANIFESTS_DIR = PathUtils.join(PathUtils.profileDir, "manifests");

// We maintain a list of scopes for installed webmanifests so we can determine
// whether a given url is within the scope of a previously installed manifest
const MANIFESTS_FILE = "manifest-scopes.json";

/**
 * Manifest object
 */

class Manifest {
  constructor(browser, manifestUrl) {
    this._manifestUrl = manifestUrl;
    // The key for this is the manifests URL that is required to be unique.
    // However arbitrary urls are not safe file paths so lets hash it.
    const fileName = generateHash(manifestUrl) + ".json";
    this._path = PathUtils.join(MANIFESTS_DIR, fileName);
    this.browser = browser;
  }

  get browser() {
    return this._browser;
  }

  set browser(aBrowser) {
    this._browser = aBrowser;
  }

  async initialize() {
    this._store = new lazy.JSONFile({ path: this._path, saveDelayMs: 100 });
    await this._store.load();
  }

  async prefetch(browser) {
    const manifestData = await ManifestObtainer.browserObtainManifest(browser);
    const icon = await ManifestIcons.browserFetchIcon(
      browser,
      manifestData,
      192
    );
    const data = {
      installed: false,
      manifest: manifestData,
      cached_icon: icon,
    };
    return data;
  }

  async install() {
    const manifestData = await ManifestObtainer.browserObtainManifest(
      this._browser
    );
    this._store.data = {
      installed: true,
      manifest: manifestData,
    };
    Manifests.manifestInstalled(this);
    this._store.saveSoon();
  }

  async icon(expectedSize) {
    if ("cached_icon" in this._store.data) {
      return this._store.data.cached_icon;
    }
    const icon = await ManifestIcons.browserFetchIcon(
      this._browser,
      this._store.data.manifest,
      expectedSize
    );
    // Cache the icon so future requests do not go over the network
    this._store.data.cached_icon = icon;
    this._store.saveSoon();
    return icon;
  }

  get scope() {
    const scope =
      this._store.data.manifest.scope || this._store.data.manifest.start_url;
    return stripQuery(scope);
  }

  get name() {
    return (
      this._store.data.manifest.short_name ||
      this._store.data.manifest.name ||
      this._store.data.manifest.short_url
    );
  }

  get url() {
    return this._manifestUrl;
  }

  get installed() {
    return (this._store.data && this._store.data.installed) || false;
  }

  get start_url() {
    return this._store.data.manifest.start_url;
  }

  get path() {
    return this._path;
  }
}

/*
 * Manifests maintains the list of installed manifests
 */
var Manifests = {
  async _initialize() {
    if (this._readyPromise) {
      return this._readyPromise;
    }

    // Prevent multiple initializations
    this._readyPromise = (async () => {
      // Make sure the manifests have the folder needed to save into
      await IOUtils.makeDirectory(MANIFESTS_DIR, { ignoreExisting: true });

      // Ensure any existing scope data we have about manifests is loaded
      this._path = PathUtils.join(PathUtils.profileDir, MANIFESTS_FILE);
      this._store = new lazy.JSONFile({ path: this._path });
      await this._store.load();

      // If we don't have any existing data, initialize empty
      if (!this._store.data.hasOwnProperty("scopes")) {
        this._store.data.scopes = new Map();
      }
    })();

    // Cache the Manifest objects creates as they are references to files
    // and we do not want multiple file handles
    this.manifestObjs = new Map();
    return this._readyPromise;
  },

  // When a manifest is installed, we save its scope so we can determine if
  // future visits fall within this manifests scope
  manifestInstalled(manifest) {
    this._store.data.scopes[manifest.scope] = manifest.url;
    this._store.saveSoon();
  },

  // Given a url, find if it is within an installed manifests scope and if so
  // return that manifests url
  findManifestUrl(url) {
    for (let scope in this._store.data.scopes) {
      if (url.startsWith(scope)) {
        return this._store.data.scopes[scope];
      }
    }
    return null;
  },

  // Get the manifest given a url, or if not look for a manifest that is
  // tied to the current page
  async getManifest(browser, manifestUrl) {
    // Ensure we have all started up
    if (!this._readyPromise) {
      await this._initialize();
    }

    // If the client does not already know its manifestUrl, we take the
    // url of the client and see if it matches the scope of any installed
    // manifests
    if (!manifestUrl) {
      const url = stripQuery(browser.currentURI.spec);
      manifestUrl = this.findManifestUrl(url);
    }

    // No matches so no manifest
    if (manifestUrl === null) {
      return null;
    }

    // If we have already created this manifest return cached
    if (this.manifestObjs.has(manifestUrl)) {
      const manifest = this.manifestObjs.get(manifestUrl);
      if (manifest.browser !== browser) {
        manifest.browser = browser;
      }
      return manifest;
    }

    // Otherwise create a new manifest object
    const manifest = new Manifest(browser, manifestUrl);
    this.manifestObjs.set(manifestUrl, manifest);
    await manifest.initialize();
    return manifest;
  },
};

var EXPORTED_SYMBOLS = ["Manifests"];
