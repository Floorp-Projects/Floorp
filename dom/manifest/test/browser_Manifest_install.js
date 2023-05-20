"use strict";

const { Manifests } = ChromeUtils.importESModule(
  "resource://gre/modules/Manifest.sys.mjs"
);

const defaultURL = new URL(
  "http://example.org/browser/dom/manifest/test/resource.sjs"
);
defaultURL.searchParams.set("Content-Type", "application/manifest+json");

const manifestMock = JSON.stringify({
  short_name: "hello World",
  scope: "/browser/",
});
const manifestUrl = `${defaultURL}&body=${manifestMock}`;

function makeTestURL() {
  const url = new URL(defaultURL);
  const body = `<link rel="manifest" href='${manifestUrl}'>`;
  url.searchParams.set("Content-Type", "text/html; charset=utf-8");
  url.searchParams.set("body", encodeURIComponent(body));
  return url.href;
}

add_task(async function () {
  const tabOptions = { gBrowser, url: makeTestURL() };

  await BrowserTestUtils.withNewTab(tabOptions, async function (browser) {
    let manifest = await Manifests.getManifest(browser, manifestUrl);
    is(manifest.installed, false, "We haven't installed this manifest yet");

    await manifest.install(browser);
    is(manifest.name, "hello World", "Manifest has correct name");
    is(manifest.installed, true, "Manifest is installed");
    is(manifest.url, manifestUrl, "has correct url");
    is(manifest.browser, browser, "has correct browser");

    manifest = await Manifests.getManifest(browser, manifestUrl);
    is(manifest.installed, true, "New instances are installed");

    manifest = await Manifests.getManifest(browser);
    is(manifest.installed, true, "Will find manifest without being given url");

    let foundManifest = Manifests.findManifestUrl(
      "http://example.org/browser/dom/"
    );
    is(foundManifest, manifestUrl, "Finds manifests within scope");

    foundManifest = Manifests.findManifestUrl("http://example.org/");
    is(foundManifest, null, "Does not find manifests outside scope");
  });
  // Get the cached one now
  await BrowserTestUtils.withNewTab(tabOptions, async browser => {
    const manifest = await Manifests.getManifest(browser, manifestUrl);
    is(manifest.browser, browser, "has updated browser object");
  });
});
