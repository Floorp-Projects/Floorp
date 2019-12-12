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
});

const uri = spec => Services.io.newURI(spec);

Services.prefs.setBoolPref("browser.ssb.enabled", true);

function parseManifest(doc, manifest = {}) {
  return ManifestProcessor.process({
    jsonText: JSON.stringify(manifest),
    manifestURL: new URL("/manifest.json", doc),
    docURL: doc,
  });
}
