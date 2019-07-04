const {PanelTestProvider} = ChromeUtils.import("resource://activity-stream/lib/PanelTestProvider.jsm");
const {BookmarkPanelHub} = ChromeUtils.import("resource://activity-stream/lib/BookmarkPanelHub.jsm");

add_task(async function test_fxa_message_shown() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async () => {
    await clearHistoryAndBookmarks();
    BrowserTestUtils.removeTab(tab);
  });

  const testURL = "data:text/plain,test cfr fxa bookmark panel message";
  const browser = gBrowser.selectedBrowser;

  await SpecialPowers.pushPrefEnv({set: [["browser.newtabpage.activity-stream.asrouter.devtoolsEnabled", false]]});
  BrowserTestUtils.loadURI(browser, testURL);
  await BrowserTestUtils.browserLoaded(browser, false, testURL);

  const [msg] = PanelTestProvider.getMessages();
  const response = BookmarkPanelHub.onResponse(msg, {
    container: document.getElementById("editBookmarkPanelRecommendation"),
    infoButton: document.getElementById("editBookmarkPanelInfoButton"),
    recommendationContainer: document.getElementById("editBookmarkPanelRecommendation"),
    url: testURL,
    document,
  }, window);

  Assert.ok(response, "We sent a valid message");

  const popupShownPromise = BrowserTestUtils.waitForEvent(StarUI.panel, "popupshown");

  // Wait for the bookmark panel state to settle and be ready to open the panel
  await BrowserTestUtils.waitForCondition(() => BookmarkingUI.status !== BookmarkingUI.STATUS_UPDATING);

  BookmarkingUI.star.click();

  await popupShownPromise;

  await BrowserTestUtils.waitForCondition(() => document.getElementById("cfrMessageContainer"), `Should create a
    container for the message`);
  for (const selector of ["#cfrClose",
      "#editBookmarkPanelRecommendationTitle",
      "#editBookmarkPanelRecommendationContent",
      "#editBookmarkPanelRecommendationCta"]) {
    Assert.ok(document.getElementById("cfrMessageContainer").querySelector(selector),
      `Should contain ${selector}`);
  }

  const ftlFiles = Array.from(document.querySelectorAll("link"))
    .filter(l => l.getAttribute("href") === "browser/newtab/asrouter.ftl" ||
      l.getAttribute("href") === "browser/branding/sync-brand.ftl");

  Assert.equal(ftlFiles.length, 2, "Two fluent files required for translating the message");

  const popupHiddenPromise = BrowserTestUtils.waitForEvent(StarUI.panel, "popuphidden");

  let removeButton = document.getElementById("editBookmarkPanelRemoveButton");

  removeButton.click();

  await popupHiddenPromise;
});
