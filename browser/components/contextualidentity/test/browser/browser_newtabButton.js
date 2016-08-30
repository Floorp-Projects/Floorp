"use strict";

// Testing that when the user opens the add tab menu and clicks menu items
// the correct context id is opened

add_task(function* test() {
  yield SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true]
  ]});

  let newTab = document.getElementById('tabbrowser-tabs');
  let newTabButton = document.getAnonymousElementByAttribute(newTab, "anonid", "tabs-newtab-button");
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");
  let popup = document.getAnonymousElementByAttribute(newTab, "anonid", "newtab-popup");

  for (let i = 1; i <= 4; i++) {
    let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(newTabButton, {type: "mousedown"});

    yield popupShownPromise;
    let contextIdItem = popup.querySelector(`menuitem[usercontextid="${i}"]`);

    ok(contextIdItem, `User context id ${i} exists`);

    let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    EventUtils.synthesizeMouseAtCenter(contextIdItem, {});

    let tab = yield waitForTabPromise;

    is(tab.getAttribute('usercontextid'), i, `New tab has UCI equal ${i}`);
    yield BrowserTestUtils.removeTab(tab);
  }
});
