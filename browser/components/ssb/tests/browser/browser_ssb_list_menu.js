/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the menu is updated correctly and can be used to launch an ssb.
add_task(async () => {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  let button = win.document.getElementById("appMenu-ssb-button");

  Assert.equal(button, null, "Button should be not be available.");
  Assert.equal(
    win.document.querySelector("#appMenu-SSBView .panel-subview-body"),
    null,
    "Panel should not be available"
  );

  let ssb = await SiteSpecificBrowser.createFromURI(
    Services.io.newURI(gHttpsTestRoot)
  );

  button = win.document.getElementById("appMenu-ssb-button");
  Assert.equal(button, null, "Button should be not be available.");
  Assert.equal(
    win.document.querySelector("#appMenu-SSBView .panel-subview-body"),
    null,
    "Panel should not be available"
  );

  await ssb.install();

  button = win.document.getElementById("appMenu-ssb-button");
  // Button should still be unavailable, we don't populate the list until it is
  // first opened.
  Assert.equal(button, null, "Button should be not be available.");
  Assert.equal(
    win.document.querySelector("#appMenu-SSBView .panel-subview-body"),
    null,
    "Panel should not be available"
  );

  let appMenuOpened = BrowserTestUtils.waitForEvent(
    win.document.getElementById("appMenu-popup"),
    "popupshown"
  );

  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("PanelUI-menu-button"),
    {},
    win
  );
  await appMenuOpened;

  button = win.document.getElementById("appMenu-ssb-button");
  await BrowserTestUtils.waitForAttributeRemoval("hidden", button);

  Assert.ok(!button.hidden, "Button should be visible.");

  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("appMenu-ssb-button"),
    {},
    win
  );

  let panelShown = BrowserTestUtils.waitForEvent(
    win.document.getElementById("appMenu-SSBView"),
    "ViewShown"
  );
  let panel = win.document.querySelector(
    "#appMenu-SSBView .panel-subview-body"
  );
  await panelShown;

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

  let ssbOpened = waitForSSB();
  EventUtils.synthesizeMouseAtCenter(panel.firstElementChild, {}, win);
  let ssbWin = await ssbOpened;

  Assert.equal(getBrowser(ssbWin).currentURI.spec, gHttpsTestRoot);
  await BrowserTestUtils.closeWindow(ssbWin);

  await ssb.uninstall();

  // Menu will be dynamically updating at this point.
  Assert.ok(button.hidden, "Should be no installs.");
  Assert.equal(panel.firstElementChild, null, "Should be nothing in the list.");

  await BrowserTestUtils.closeWindow(win);
});
