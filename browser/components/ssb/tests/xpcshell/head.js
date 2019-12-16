/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SiteSpecificBrowser, SiteSpecificBrowserService } = ChromeUtils.import(
  "resource:///modules/SiteSpecificBrowserService.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const ICON16 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAADUExURUdwTIL60tIAAAABdFJOUwBA5thmAAAAC0lEQVQIHWMgEQAAADAAAQrnSBQAAAAASUVORK5CYII=";
const ICON32 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgAQMAAABJtOi3AAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAADUExURUdwTIL60tIAAAABdFJOUwBA5thmAAAAC0lEQVQIHWMY5AAAAKAAAZearVIAAAAASUVORK5CYII=";
const ICON48 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwAQMAAABtzGvEAAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAADUExURUdwTIL60tIAAAABdFJOUwBA5thmAAAADElEQVQYGWMYBVQFAAFQAAHUa/NpAAAAAElFTkSuQmCC";
const ICON96 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAABgAQMAAADYVuV7AAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAADUExURUdwTIL60tIAAAABdFJOUwBA5thmAAAAEUlEQVQYGWMYBaNgFIyCYQoABOAAAZ11NUsAAAAASUVORK5CYII=";
const ICON128 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAADUExURUdwTIL60tIAAAABdFJOUwBA5thmAAAAGElEQVQYGWMYBaNgFIyCUTAKRsEooDMAAAiAAAE2cKqmAAAAAElFTkSuQmCC";
const ICON256 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAQAAAAEAAQMAAABmvDolAAAAA1BMVEVHcEyC+tLSAAAAAXRSTlMAQObYZgAAAB9JREFUGBntwQENAAAAwiD7p34ON2AAAAAAAAAAAOcCIQAAAfWivQQAAAAASUVORK5CYII=";

XPCOMUtils.defineLazyModuleGetters(this, {
  ManifestProcessor: "resource://gre/modules/ManifestProcessor.jsm",
  KeyValueService: "resource://gre/modules/kvstore.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
});

const SSB_STORE_PREFIX = "ssb:";

const uri = spec => Services.io.newURI(spec);
const storeKey = id => SSB_STORE_PREFIX + id;

let gProfD = do_get_profile();
let gSSBData = gProfD.clone();
gSSBData.append("ssb");

Services.prefs.setBoolPref("browser.ssb.enabled", true);
Services.prefs.setBoolPref("browser.ssb.osintegration", false);

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
