/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that focusing the 'New Tage Page' works as expected.
 */
function runTests() {
  // Focus count in new tab page.
  // 28 = 9 * 3 + 1 = 9 sites and 1 toggle button, each site has a link, a pin
  // and a remove button.
  let FOCUS_COUNT = 28; 
  if ("nsILocalFileMac" in Ci) {
    // 19 = Mac doesn't focus links, so 9 focus targets less than Windows/Linux.
    FOCUS_COUNT = 19;
  }

  // Create a new tab page.
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  gURLBar.focus();

  // Count the focus with the enabled page.
  yield countFocus(FOCUS_COUNT);

  // Disable page and count the focus with the disabled page.
  NewTabUtils.allPages.enabled = false;
  yield countFocus(1);
}

/**
 * Focus the urlbar and count how many focus stops to return again to the urlbar.
 */
function countFocus(aExpectedCount) {
  let focusCount = 0;
  let contentDoc = getContentDocument();

  window.addEventListener("focus", function onFocus() {
    let focusedElement = document.commandDispatcher.focusedElement;
    if (focusedElement && focusedElement.classList.contains("urlbar-input")) {
      window.removeEventListener("focus", onFocus, true);
      is(focusCount, aExpectedCount, "Validate focus count in the new tab page.");
      executeSoon(TestRunner.next);
    } else {
      if (focusedElement && focusedElement.ownerDocument == contentDoc &&
          focusedElement instanceof HTMLElement) {
        focusCount++;
      }
      document.commandDispatcher.advanceFocus();
    }
  }, true);

  document.commandDispatcher.advanceFocus();
}
