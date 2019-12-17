/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SiteSpecificBrowser, SiteSpecificBrowserService } = ChromeUtils.import(
  "resource:///modules/SiteSpecificBrowserService.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ManifestProcessor: "resource://gre/modules/ManifestProcessor.jsm",
  KeyValueService: "resource://gre/modules/kvstore.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

const SSB_STORE_PREFIX = "ssb:";

const uri = spec => Services.io.newURI(spec);
const storeKey = id => SSB_STORE_PREFIX + id;

let gProfD = do_get_profile();
let gSSBData = gProfD.clone();
gSSBData.append("ssb");

Services.prefs.setBoolPref("browser.ssb.enabled", true);

async function getKVStore() {
  await OS.File.makeDir(gSSBData.path);
  return KeyValueService.getOrCreate(gSSBData.path, "ssb");
}

function parseManifest(doc, manifest = {}) {
  return ManifestProcessor.process({
    jsonText: JSON.stringify(manifest),
    manifestURL: new URL("/manifest.json", doc),
    docURL: doc,
  });
}

async function storeSSB(store, id, manifest, config = {}) {
  return store.put(
    storeKey(id),
    JSON.stringify({
      manifest,
      config,
    })
  );
}
