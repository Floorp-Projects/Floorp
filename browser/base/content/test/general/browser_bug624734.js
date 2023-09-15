/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 624734 - Star UI has no tooltip until bookmarked page is visited

function finishTest() {
  let elem = document.getElementById("context-bookmarkpage");
  let l10n = document.l10n.getAttributes(elem);
  ok(
    [
      "main-context-menu-bookmark-page",
      "main-context-menu-bookmark-page-with-shortcut",
      "main-context-menu-bookmark-page-mac",
    ].includes(l10n.id)
  );

  gBrowser.removeCurrentTab();
  finish();
}

function test() {
  waitForExplicitFinish();

  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  CustomizableUI.addWidgetToArea(
    "bookmarks-menu-button",
    CustomizableUI.AREA_NAVBAR,
    0
  );
  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
    if (BookmarkingUI.status == BookmarkingUI.STATUS_UPDATING) {
      waitForCondition(
        () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING,
        finishTest,
        "BookmarkingUI was updating for too long"
      );
    } else {
      CustomizableUI.removeWidgetFromArea("bookmarks-menu-button");
      finishTest();
    }
  });

  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/browser/browser/base/content/test/general/dummy_page.html"
  );
}
