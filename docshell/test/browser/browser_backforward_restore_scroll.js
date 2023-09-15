/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);
const URL1 = ROOT + "file_backforward_restore_scroll.html";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const URL2 = "http://example.net/";

const SCROLL0 = 500;
const SCROLL1 = 1000;

function promiseBrowserLoaded(url) {
  return BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, url);
}

add_task(async function test() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URL1);

  // Scroll the 2 frames.
  let children = gBrowser.selectedBrowser.browsingContext.children;
  await SpecialPowers.spawn(children[0], [SCROLL0], scrollY =>
    content.scrollTo(0, scrollY)
  );
  await SpecialPowers.spawn(children[1], [SCROLL1], scrollY =>
    content.scrollTo(0, scrollY)
  );

  // Navigate forwards then backwards.
  let loaded = promiseBrowserLoaded(URL2);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, URL2);
  await loaded;

  loaded = promiseBrowserLoaded(URL1);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.history.back();
  });
  await loaded;

  // And check the results.
  children = gBrowser.selectedBrowser.browsingContext.children;
  await SpecialPowers.spawn(children[0], [SCROLL0], scrollY => {
    Assert.equal(content.scrollY, scrollY, "frame 0 has correct scroll");
  });
  await SpecialPowers.spawn(children[1], [SCROLL1], scrollY => {
    Assert.equal(content.scrollY, scrollY, "frame 1 has correct scroll");
  });

  gBrowser.removeTab(gBrowser.selectedTab);
});
