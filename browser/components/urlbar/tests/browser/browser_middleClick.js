/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test for middle click behavior.
 */

add_task(function test_setup() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("middlemouse.paste");
    SpecialPowers.clipboardCopyString("");
  });
});

add_task(async function test_middleClickOnTab() {
  await testMiddleClickOnTab(false);
  await testMiddleClickOnTab(true);
});

add_task(async function test_middleClickOnURLBar() {
  await testMiddleClickOnURLBar(false);
  await testMiddleClickOnURLBar(true);
});

async function testMiddleClickOnTab(isMiddleMousePastePrefOn) {
  SpecialPowers.clipboardCopyString("test\nsample");

  info(`Set middlemouse.paste [${isMiddleMousePastePrefOn}]`);
  Services.prefs.setBoolPref("middlemouse.paste", isMiddleMousePastePrefOn);

  info("Open two tabs");
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Middle click on tab2 to remove it");
  EventUtils.synthesizeMouseAtCenter(tab2, { button: 1 });

  info("Wait until the tab1 is selected");
  await TestUtils.waitForCondition(() => gBrowser.selectedTab === tab1);

  Assert.equal(gURLBar.value, "", "URLBar has no pasted value");

  BrowserTestUtils.removeTab(tab1);
}

async function testMiddleClickOnURLBar(isMiddleMousePastePrefOn) {
  SpecialPowers.clipboardCopyString("test\nsample");
  gURLBar.value = "";

  info(`Set middlemouse.paste [${isMiddleMousePastePrefOn}]`);
  Services.prefs.setBoolPref("middlemouse.paste", isMiddleMousePastePrefOn);

  info("Middle click on the urlbar");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { button: 1 });

  if (isMiddleMousePastePrefOn) {
    Assert.equal(gURLBar.value, "test sample", "URLBar has pasted value");
  } else {
    Assert.equal(gURLBar.value, "", "URLBar has no pasted value");
  }
}
