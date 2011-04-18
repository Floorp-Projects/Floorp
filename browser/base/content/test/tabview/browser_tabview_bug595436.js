/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw, search, searchButton;

  let assertSearchIsEnabled = function () {
    isnot(search.style.display, "none", "search is enabled");
  }

  let assertSearchIsDisabled = function () {
    is(search.style.display, "none", "search is disabled");
  }

  let testSearchInitiatedByKeyPress = function () {
    EventUtils.synthesizeKey("a", {}, cw);
    assertSearchIsEnabled();

    EventUtils.synthesizeKey("VK_BACK_SPACE", {}, cw);
    assertSearchIsDisabled();
  }

  let testSearchInitiatedByMouseClick = function () {
    EventUtils.sendMouseEvent({type: "mousedown"}, searchButton, cw);
    assertSearchIsEnabled();

    EventUtils.synthesizeKey("a", {}, cw);
    EventUtils.synthesizeKey("VK_BACK_SPACE", {}, cw);
    EventUtils.synthesizeKey("VK_BACK_SPACE", {}, cw);
    assertSearchIsEnabled();

    EventUtils.synthesizeKey("VK_ESCAPE", {}, cw);
    assertSearchIsDisabled();
  }

  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());

    cw = win.TabView.getContentWindow();
    search = cw.document.getElementById("search");
    searchButton = cw.document.getElementById("searchbutton");

    SimpleTest.waitForFocus(function () {
      assertSearchIsDisabled();

      testSearchInitiatedByKeyPress();
      testSearchInitiatedByMouseClick();

      finish();
    }, cw);
  });
}
