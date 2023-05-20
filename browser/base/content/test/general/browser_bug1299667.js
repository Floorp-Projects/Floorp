/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.history.pushState({}, "2", "2.html");
  });

  await TestUtils.topicObserved("sessionstore-state-write-complete");

  // Wait for the session data to be flushed before continuing the test
  await new Promise(resolve =>
    SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
  );

  let backButton = document.getElementById("back-button");
  let contextMenu = document.getElementById("backForwardMenu");

  info("waiting for the history menu to open");

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(backButton, {
    type: "contextmenu",
    button: 2,
  });
  let event = await popupShownPromise;

  ok(true, "history menu opened");

  // Wait for the session data to be flushed before continuing the test
  await new Promise(resolve =>
    SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
  );

  is(event.target.children.length, 2, "Two history items");

  let node = event.target.firstElementChild;
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  is(node.getAttribute("uri"), "http://example.com/2.html", "first item uri");
  is(node.getAttribute("index"), "1", "first item index");
  is(node.getAttribute("historyindex"), "0", "first item historyindex");

  node = event.target.lastElementChild;
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  is(node.getAttribute("uri"), "http://example.com/", "second item uri");
  is(node.getAttribute("index"), "0", "second item index");
  is(node.getAttribute("historyindex"), "-1", "second item historyindex");

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  event.target.hidePopup();
  await popupHiddenPromise;
  info("Hidden popup");

  let onClose = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabClose"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await onClose;
  info("Tab closed");
});
