/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var originalWindowHeight;
registerCleanupFunction(function() {
  window.resizeTo(window.outerWidth, originalWindowHeight);
  while (gBrowser.tabs[1]) {
    gBrowser.removeTab(gBrowser.tabs[1]);
  }
});

add_task(async function() {
  originalWindowHeight = window.outerHeight;
  window.resizeTo(window.outerWidth, 300);
  let prefs = await openPreferencesViaOpenPreferencesAPI("paneSearch", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneSearch", "Search pane was selected");
  let mainContent = gBrowser.contentDocument.querySelector(".main-content");
  mainContent.scrollTop = 50;
  is(mainContent.scrollTop, 50, "main-content should be scrolled 50 pixels");

  await gBrowser.contentWindow.gotoPref("paneGeneral");

  is(
    mainContent.scrollTop,
    0,
    "Switching to a different category should reset the scroll position"
  );
});
