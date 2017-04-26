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

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const { ManifestObtainer } =
  Cu.import("resource://gre/modules/ManifestObtainer.jsm", {});
const { ManifestIcons } =
  Cu.import("resource://gre/modules/ManifestIcons.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");

/**
 * Generates an hash for the given string.
 *
 * @note The generated hash is returned in base64 form.  Mind the fact base64
 * is case-sensitive if you are going to reuse this code.
 */
function generateHash(aString) {
  const cryptoHash = Cc["@mozilla.org/security/hash;1"]
    .createInstance(Ci.nsICryptoHash);
  cryptoHash.init(Ci.nsICryptoHash.MD5);
  const stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
    .createInstance(Ci.nsIStringInputStream);
  stringStream.data = aString;
  cryptoHash.updateFromStream(stringStream, -1);
  // base64 allows the '/' char, but we can't use it for filenames.
  return cryptoHash.finish(true).replace(/\//g, "-");
}

/**
 * Trims the query paramters from a url
 */
function stripQuery(url) {
  return url.split("?")[0];
}

// Folder in which we store the manifest files
const MANIFESTS_DIR = OS.Path.join(OS.Constants.Path.profileDir, "manifests");

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
    this._path = OS.Path.join(MANIFESTS_DIR, fileName);
    this._browser = browser;
  }

  async initialise() {
    this._store = new JSONFile({path: this._path});
    await this._store.load();
  }

  async install() {
    const manifestData = await ManifestObtainer.browserObtainManifest(this._browser);
    this._store.data = {
      installed: true,
      manifest: manifestData
    };
    Manifests.manifestInstalled(this);
    this._store.saveSoon();
  }

  async icon(expectedSize) {
    if ('cached_icon' in this._store.data) {
      return this._store.data.cached_icon;
    }
    const icon = await ManifestIcons
      .browserFetchIcon(this._browser, this._store.data.manifest, expectedSize);
    // Cache the icon so future requests do not go over the network
    this._store.data.cached_icon = icon;
    this._store.saveSoon();
    return icon;
  }

  get scope() {
    const scope = this._store.data.manifest.scope ||
      this._store.data.manifest.start_url;
    return stripQuery(scope);
  }

  get name() {
    return this._store.data.manifest.short_name ||
      this._store.data.manifest.short_url;
  }

  get url() {
    return this._manifestUrl;
  }

  get installed() {
    return this._store.data && this._store.data.installed || false;
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

  async initialise () {

    if (this.started) {
      return this.started;
    }

    this.started = (async () => {

      // Make sure the manifests have the folder needed to save into
      await OS.File.makeDir(MANIFESTS_DIR, {ignoreExisting: true});

      // Ensure any existing scope data we have about manifests is loaded
      this._path = OS.Path.join(OS.Constants.Path.profileDir, MANIFESTS_FILE);
      this._store = new JSONFile({path: this._path});
      await this._store.load();

      // If we dont have any existing data, initialise empty
      if (!this._store.data.hasOwnProperty("scopes")) {
        this._store.data.scopes = new Map();
      }

      // Cache the Manifest objects creates as they are references to files
      // and we do not want multiple file handles
      this.manifestObjs = {};

    })();

    return this.started;
  },

  // When a manifest is installed, we save its scope so we can determine if
  // fiture visits fall within this manifests scope
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
    await this.initialise();

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
    if (manifestUrl in this.manifestObjs) {
      return this.manifestObjs[manifestUrl];
    }

    // Otherwise create a new manifest object
    this.manifestObjs[manifestUrl] = new Manifest(browser, manifestUrl);
    await this.manifestObjs[manifestUrl].initialise();
    return this.manifestObjs[manifestUrl];
  }

};

this.EXPORTED_SYMBOLS = ["Manifests"]; // jshint ignore:line
