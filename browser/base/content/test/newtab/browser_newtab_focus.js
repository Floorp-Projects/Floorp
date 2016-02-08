/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that focusing the 'New Tab Page' works as expected.
 */
add_task(function* () {
  yield pushPrefs(["accessibility.tabfocus", 7]);

  // Focus count in new tab page.
  // 30 = 9 * 3 + 3 = 9 sites, each with link, pin and remove buttons; search
  // bar; search button; and toggle button. Additionaly there may or may not be
  // a scroll bar caused by fix to 1180387, which will eat an extra focus
  let FOCUS_COUNT = 30;

  // Create a new tab page.
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield* addNewTabPageTab();
  gURLBar.focus();

  // Count the focus with the enabled page.
  countFocus(FOCUS_COUNT);

  // Disable page and count the focus with the disabled page.
  NewTabUtils.allPages.enabled = false;

  countFocus(4);

  NewTabUtils.allPages.enabled = true;
});

/**
 * Focus the urlbar and count how many focus stops to return again to the urlbar.
 */
function countFocus(aExpectedCount) {
  let focusCount = 0;
  do {
    EventUtils.synthesizeKey("VK_TAB", {});
    if (document.activeElement == gBrowser.selectedBrowser) {
      focusCount++;
    }
  } while (document.activeElement != gURLBar.inputField);

  ok(focusCount == aExpectedCount || focusCount == (aExpectedCount + 1),
     "Validate focus count in the new tab page.");
}
