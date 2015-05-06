/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.history.pushState({}, "2", "2.html");
  });

  var backButton = document.getElementById("back-button");
  var rect = backButton.getBoundingClientRect();

  info("waiting for the history menu to open");

  let popupShownPromise = BrowserTestUtils.waitForEvent(backButton, "popupshown");
  EventUtils.synthesizeMouseAtCenter(backButton, {type: "mousedown"});
  EventUtils.synthesizeMouse(backButton, rect.width / 2, rect.height, {type: "mouseup"});
  let event = yield popupShownPromise;

  ok(true, "history menu opened");

  event.target.hidePopup();
  gBrowser.removeTab(gBrowser.selectedTab);
});
