/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the session history can be shown by long-pressing the back button.
// And that middle-click opens one tab (as a regression test for bug 1657992).
add_task(async function restore_history_entry_by_middle_click() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );

  await SpecialPowers.spawn(tab1.linkedBrowser, [], () => {
    content.history.pushState(null, null, "2.html");
    content.history.pushState(null, null, "3.html");
  });

  await new Promise(resolve => SessionStore.getSessionHistory(tab1, resolve));

  let backButton = document.getElementById("back-button");
  // This is the popup (clone of backForwardMenu) from SetClickAndHoldHandlers.
  let historyMenu = backButton.firstElementChild;

  info("waiting for the history menu to open");

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    historyMenu,
    "popupshown"
  );

  // Trigger gClickAndHoldListenersOnElement logic in browser.js to open the
  // history menu that opens after a long press.
  EventUtils.synthesizeMouseAtCenter(backButton, { type: "mousedown" });
  let event = await popupShownPromise;
  EventUtils.synthesizeMouseAtCenter(backButton, { type: "mouseup" });

  info("Waiting for menu items to be populated");
  await new Promise(resolve => SessionStore.getSessionHistory(tab1, resolve));

  SimpleTest.isDeeply(
    Array.from(event.target.children, node => node.getAttribute("uri")),
    [
      "http://example.com/3.html",
      "http://example.com/2.html",
      "http://example.com/",
    ],
    "Expected session history items"
  );
  let historyMenuItem = event.target.children[1];

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    historyMenu,
    "popuphidden"
  );

  let tabRestoredPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "SSTabRestored"
  );

  EventUtils.sendMouseEvent({ type: "click", button: 1 }, historyMenuItem);

  info("Waiting for history menu to be hidden");
  await popupHiddenPromise;
  info("Waiting for history item to be restored in a new tab");
  let newTab = (await tabRestoredPromise).target;
  is(newTab.linkedBrowser.currentURI.spec, "http://example.com/2.html");

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab1);
});
