/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);
SearchTestUtils.init(this);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.w3c_touch_events.enabled", 0]],
  });
});

add_task(async function test_scroll() {
  info("Open preferences page for search");
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  const doc = gBrowser.selectedBrowser.contentDocument;
  const tree = doc.querySelector("#engineList");

  info("Add engines to make the tree scrollable");
  for (let i = 0, n = parseInt(tree.getAttribute("rows")); i < n; i++) {
    let extension = await SearchTestUtils.installSearchExtension({
      id: `${i}@tests.mozilla.org`,
      name: `example${i}`,
      version: "1.0",
      keyword: `example${i}`,
    });
    await AddonTestUtils.waitForSearchProviderStartup(extension);
  }

  info("Make tree element move into viewport");
  const mainContent = doc.querySelector(".main-content");
  const readyForScrollIntoView = new Promise(r => {
    mainContent.addEventListener("scroll", r, { once: true });
  });
  tree.scrollIntoView();
  await readyForScrollIntoView;

  const previousScroll = mainContent.scrollTop;

  await promiseMoveMouseAndScrollWheelOver(tree, 1, 1, false);

  Assert.equal(
    previousScroll,
    mainContent.scrollTop,
    "Container element does not scroll"
  );

  info("Clean up");
  gBrowser.removeCurrentTab();
});
