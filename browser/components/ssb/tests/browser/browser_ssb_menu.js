/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that it is correctly enabled/disabled based on the displaying page.
add_task(async () => {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: gHttpTestRoot + "test_page.html",
  });

  // Must open the panel before the item gets added.
  let pageActionButton = document.getElementById("pageActionButton");
  let panel = document.getElementById("pageActionPanel");
  let popupShown = BrowserTestUtils.waitForEvent(panel, "popupshown");

  EventUtils.synthesizeMouseAtCenter(pageActionButton, {}, window);
  await popupShown;

  let popupHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
  panel.hidePopup();
  await popupHidden;

  Assert.ok(
    document.getElementById("pageAction-panel-launchSSB").disabled,
    "Menu should be disabled for a http: page."
  );

  let uri = gHttpsTestRoot + "test_page.html";
  BrowserTestUtils.loadURI(tab.linkedBrowser, uri);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, true, uri);

  Assert.ok(
    !document.getElementById("pageAction-panel-launchSSB").disabled,
    "Menu should not be disabled for a https: page."
  );

  BrowserTestUtils.removeTab(tab);
});
