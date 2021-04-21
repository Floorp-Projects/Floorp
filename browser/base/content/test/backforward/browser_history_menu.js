/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that the back forward button long-press menu and context menu
// shows the correct history items.

add_task(async function mousedown_back() {
  await testBackForwardMenu(false);
});

add_task(async function contextmenu_back() {
  await testBackForwardMenu(true);
});

async function testBackForwardMenu(useContextMenu) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );

  for (let iter = 2; iter <= 4; iter++) {
    // Iterate three times. For the first two times through the loop, add a new history item.
    // But for the last iteration, go back in the history instead.
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [iter], async function(
      iterChild
    ) {
      if (iterChild == 4) {
        content.history.back();
      } else {
        content.history.pushState({}, "" + iterChild, iterChild + ".html");
      }
    });

    // Wait for the session data to be flushed before continuing the test
    await new Promise(resolve =>
      SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
    );

    let backButton = document.getElementById("back-button");
    let rect = backButton.getBoundingClientRect();

    info("waiting for the history menu to open");

    let popupShownPromise = BrowserTestUtils.waitForEvent(
      useContextMenu ? document.getElementById("backForwardMenu") : backButton,
      "popupshown"
    );
    if (useContextMenu) {
      EventUtils.synthesizeMouseAtCenter(backButton, {
        type: "contextmenu",
        button: 2,
      });
    } else {
      EventUtils.synthesizeMouseAtCenter(backButton, { type: "mousedown" });
    }

    EventUtils.synthesizeMouse(backButton, rect.width / 2, rect.height, {
      type: "mouseup",
    });
    let popupEvent = await popupShownPromise;

    ok(true, "history menu opened");

    // Wait for the session data to be flushed before continuing the test
    await new Promise(resolve =>
      SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
    );

    is(
      popupEvent.target.children.length,
      iter > 3 ? 3 : iter,
      "Correct number of history items"
    );

    let node = popupEvent.target.lastElementChild;
    is(node.getAttribute("uri"), "http://example.com/", "'1' item uri");
    is(node.getAttribute("index"), "0", "'1' item index");
    is(
      node.getAttribute("historyindex"),
      iter == 3 ? "-2" : "-1",
      "'1' item historyindex"
    );

    node = node.previousElementSibling;
    is(node.getAttribute("uri"), "http://example.com/2.html", "'2' item uri");
    is(node.getAttribute("index"), "1", "'2' item index");
    is(
      node.getAttribute("historyindex"),
      iter == 3 ? "-1" : "0",
      "'2' item historyindex"
    );

    if (iter >= 3) {
      node = node.previousElementSibling;
      is(node.getAttribute("uri"), "http://example.com/3.html", "'3' item uri");
      is(node.getAttribute("index"), "2", "'3' item index");
      is(
        node.getAttribute("historyindex"),
        iter == 4 ? "1" : "0",
        "'3' item historyindex"
      );
    }

    // Close the popup, but on the last iteration, click on one of the history items
    // to ensure it opens in a new tab.
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      popupEvent.target,
      "popuphidden"
    );

    if (iter < 4) {
      popupEvent.target.hidePopup();
    } else {
      let newTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        url => url == "http://example.com/"
      );

      popupEvent.target.activateItem(popupEvent.target.children[2], {
        button: 1,
      });

      let newtab = await newTabPromise;
      gBrowser.removeTab(newtab);
    }

    await popupHiddenPromise;
  }

  gBrowser.removeTab(tab);
}
