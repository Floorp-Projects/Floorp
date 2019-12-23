/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the menu is updated correctly and can be used to launch an ssb.
add_task(async () => {
  let button = document.getElementById("appMenu-ssb-button");
  let panel = document.querySelector("#appMenu-SSBView .panel-subview-body");

  Assert.ok(button.hidden, "Button should be hidden.");
  Assert.equal(panel.firstElementChild, null, "Should be nothing in the list.");

  let ssb = await SiteSpecificBrowser.createFromURI(
    Services.io.newURI(gHttpsTestRoot)
  );

  Assert.ok(button.hidden, "Button should be hidden.");
  Assert.equal(panel.firstElementChild, null, "Should be nothing in the list.");

  await ssb.install();

  // Button should still be hidden, we don't populate the list until it is
  // first opened.
  Assert.ok(button.hidden, "Button should be hidden.");
  Assert.equal(panel.firstElementChild, null, "Should be nothing in the list.");

  let appMenuOpened = BrowserTestUtils.waitForEvent(
    document.getElementById("appMenu-popup"),
    "popupshown"
  );

  let buttonShown = BrowserTestUtils.waitForAttributeRemoval("hidden", button);

  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("PanelUI-menu-button"),
    {}
  );
  await Promise.all([appMenuOpened, buttonShown]);

  Assert.ok(!button.hidden, "Button should be visible.");
  Assert.notEqual(
    panel.firstElementChild,
    null,
    "Should be something in the list."
  );
  Assert.equal(
    panel.firstElementChild.id,
    "ssb-button-" + ssb.id,
    "Should have the right ID."
  );

  let panelShown = BrowserTestUtils.waitForEvent(
    document.getElementById("appMenu-SSBView"),
    "ViewShown"
  );

  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("appMenu-ssb-button"),
    {},
    window
  );
  await panelShown;

  let ssbOpened = waitForSSB();
  EventUtils.synthesizeMouseAtCenter(panel.firstElementChild, {}, window);
  let ssbWin = await ssbOpened;

  Assert.equal(getBrowser(ssbWin).currentURI.spec, gHttpsTestRoot);
  await BrowserTestUtils.closeWindow(ssbWin);

  await ssb.uninstall();

  // Menu will be dynamically updating at this point.
  Assert.ok(button.hidden, "Should be no installs.");
  Assert.equal(panel.firstElementChild, null, "Should be nothing in the list.");
});
