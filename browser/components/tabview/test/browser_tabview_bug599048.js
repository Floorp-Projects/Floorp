/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let cw;

function test() {
  waitForExplicitFinish();

  showTabView(function() {
    cw = TabView.getContentWindow();

    whenSearchIsEnabled(function() {
      ok(cw.Search.isEnabled(), "The search is disabled");

      // open a new window and it would have the focus
      newWindowWithTabView(function(win) {
        registerCleanupFunction(function() {
          win.close();
          hideTabView();
        });
        testClickOnSearchShade(win);
      });
    });

    EventUtils.synthesizeKey("VK_SLASH", {}, cw);
  });
}

function testClickOnSearchShade(win) {
  // click on the window with search enabled.
  let searchshade = cw.document.getElementById("searchshade");
  EventUtils.sendMouseEvent({ type: "click" }, searchshade, cw);

  waitForFocus(function() {
    ok(cw.Search.isEnabled(), "The search is still enabled after the search shade is clicked");
    testFocusInactiveWindow(win, cw);
  });
}

function testFocusInactiveWindow(win) {
  win.focus();
  // focus inactive window
  window.focus();

  // need to use exeuteSoon as the _blockClick would be set to false after a setTimeout(,0)
  executeSoon(function() {
    ok(cw.Search.isEnabled(), "The search is still enabled when inactive window has focus");

    whenSearchIsDisabled(function() {
      hideTabView(finish);
    });

    let searchshade = cw.document.getElementById("searchshade");
    EventUtils.synthesizeMouseAtCenter(searchshade, {}, cw);
});
}

