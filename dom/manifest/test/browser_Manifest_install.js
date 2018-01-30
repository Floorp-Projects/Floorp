//Used by JSHint:
/*global Cu, BrowserTestUtils, ok, add_task, gBrowser */
"use strict";

const { Manifests } = Cu.import("resource://gre/modules/Manifest.jsm", {});

const defaultURL = new URL("http://example.org/browser/dom/manifest/test/resource.sjs");
defaultURL.searchParams.set("Content-Type", "application/manifest+json");

const manifest = JSON.stringify({short_name: "hello World", scope: "/browser/"});
const manifestUrl = `${defaultURL}&body=${manifest}`;

function makeTestURL(manifest) {
  const url = new URL(defaultURL);
  const body = `<link rel="manifest" href='${manifestUrl}'>`;
  url.searchParams.set("Content-Type", "text/html; charset=utf-8");
  url.searchParams.set("body", encodeURIComponent(body));
  return url.href;
}

add_task(async function() {

  const tabOptions = {gBrowser, url: makeTestURL(manifest)};

  await BrowserTestUtils.withNewTab(tabOptions, async function(browser) {

    let manifest = await Manifests.getManifest(browser, manifestUrl);
    is(manifest.installed, false, "We havent installed this manifest yet");

    await manifest.install(browser);
    is(manifest.name, "hello World", "Manifest has correct name");
    is(manifest.installed, true, "Manifest is installed");
    is(manifest.url, manifestUrl, "has correct url");

    manifest = await Manifests.getManifest(browser, manifestUrl);
    is(manifest.installed, true, "New instances are installed");

    manifest = await Manifests.getManifest(browser);
    is(manifest.installed, true, "Will find manifest without being given url");

    let foundManifest = Manifests.findManifestUrl("http://example.org/browser/dom/");
    is(foundManifest, manifestUrl, "Finds manifests within scope");

    foundManifest = Manifests.findManifestUrl("http://example.org/");
    is(foundManifest, null, "Does not find manifests outside scope");
  });

});

