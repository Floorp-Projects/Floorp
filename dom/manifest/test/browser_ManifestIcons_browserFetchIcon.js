//Used by JSHint:
/*global Cu, BrowserTestUtils, ok, add_task, gBrowser */
"use strict";
const { ManifestIcons } = Cu.import("resource://gre/modules/ManifestIcons.jsm", {});
const { ManifestObtainer } = Cu.import("resource://gre/modules/ManifestObtainer.jsm", {});

const defaultURL = new URL("http://example.org/browser/dom/manifest/test/resource.sjs");
defaultURL.searchParams.set("Content-Type", "application/manifest+json");

const manifest = JSON.stringify({
  icons: [{
    sizes: "50x50",
    src: "red-50.png?Content-type=image/png"
  }, {
    sizes: "150x150",
    src: "blue-150.png?Content-type=image/png"
  }]
});

function makeTestURL(manifest) {
  const url = new URL(defaultURL);
  const body = `<link rel="manifest" href='${defaultURL}&body=${manifest}'>`;
  url.searchParams.set("Content-Type", "text/html; charset=utf-8");
  url.searchParams.set("body", encodeURIComponent(body));
  return url.href;
}

function getIconColor(icon) {
  return new Promise((resolve, reject) => {
    const canvas = content.document.createElement('canvas');
    const ctx = canvas.getContext("2d");
    const image = new content.Image();
    image.onload = function() {
      ctx.drawImage(image, 0, 0);
      resolve(ctx.getImageData(1, 1, 1, 1).data);
    };
    image.onerror = function() {
      reject(new Error("could not create image"));
    };
    image.src = icon;
  });
}

add_task(async function() {
  const tabOptions = {gBrowser, url: makeTestURL(manifest)};
  await BrowserTestUtils.withNewTab(tabOptions, async function(browser) {
    const manifest = await ManifestObtainer.browserObtainManifest(browser);
    let icon = await ManifestIcons.browserFetchIcon(browser, manifest, 25);
    let color = await ContentTask.spawn(browser, icon, getIconColor);
    is(color[0], 255, 'Fetched red icon');

    icon = await ManifestIcons.browserFetchIcon(browser, manifest, 500);
    color = await ContentTask.spawn(browser, icon, getIconColor);
    is(color[2], 255, 'Fetched blue icon');
  });
});
