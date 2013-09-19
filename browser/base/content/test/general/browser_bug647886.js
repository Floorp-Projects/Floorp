/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    content.history.pushState({}, "2", "2.html");

    testBackButton();
  }, true);

  loadURI("http://example.com");
}

function testBackButton() {
  var backButton = document.getElementById("back-button");
  var rect = backButton.getBoundingClientRect();

  info("waiting for the history menu to open");

  backButton.addEventListener("popupshown", function (event) {
    backButton.removeEventListener("popupshown", arguments.callee, false);

    ok(true, "history menu opened");
    event.target.hidePopup();
    gBrowser.removeTab(gBrowser.selectedTab);
    finish();
  }, false);

  EventUtils.synthesizeMouseAtCenter(backButton, {type: "mousedown"});
  EventUtils.synthesizeMouse(backButton, rect.width / 2, rect.height, {type: "mouseup"});
}
