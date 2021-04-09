/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test for middle click behavior.
 */

add_task(function test_setup() {
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("home-button", "nav-bar");
  }

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("middlemouse.paste");
    Services.prefs.clearUserPref("middlemouse.openNewWindow");
    Services.prefs.clearUserPref("browser.tabs.opentabfor.middleclick");
    Services.prefs.clearUserPref("browser.startup.homepage");
    Services.prefs.clearUserPref("browser.tabs.loadBookmarksInBackground");
    SpecialPowers.clipboardCopyString("");

    if (CustomizableUI.protonToolbarEnabled) {
      CustomizableUI.removeWidgetFromArea("home-button");
    }
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

add_task(async function test_middleClickOnHomeButton() {
  const TEST_DATA = [
    {
      isMiddleMousePastePrefOn: false,
      isLoadInBackground: false,
      startPagePref: "about:home",
      expectedURLBarFocus: true,
      expectedURLBarValue: "",
    },
    {
      isMiddleMousePastePrefOn: false,
      isLoadInBackground: false,
      startPagePref: "about:blank",
      expectedURLBarFocus: true,
      expectedURLBarValue: "",
    },
    {
      isMiddleMousePastePrefOn: false,
      isLoadInBackground: false,
      startPagePref: "https://example.com",
      expectedURLBarFocus: false,
      expectedURLBarValue: "https://example.com",
    },
    {
      isMiddleMousePastePrefOn: true,
      isLoadInBackground: false,
      startPagePref: "about:home",
      expectedURLBarFocus: true,
      expectedURLBarValue: "",
    },
    {
      isMiddleMousePastePrefOn: true,
      isLoadInBackground: false,
      startPagePref: "https://example.com",
      expectedURLBarFocus: false,
      expectedURLBarValue: "https://example.com",
    },
    {
      isMiddleMousePastePrefOn: false,
      isLoadInBackground: true,
      startPagePref: "about:home",
      expectedURLBarFocus: true,
      expectedURLBarValue: "",
    },
    {
      isMiddleMousePastePrefOn: false,
      isLoadInBackground: true,
      startPagePref: "https://example.com",
      expectedURLBarFocus: true,
      expectedURLBarValue: "",
    },
    {
      isMiddleMousePastePrefOn: true,
      isLoadInBackground: true,
      startPagePref: "about:home",
      expectedURLBarFocus: true,
      expectedURLBarValue: "",
    },
    {
      isMiddleMousePastePrefOn: true,
      isLoadInBackground: true,
      startPagePref: "https://example.com",
      expectedURLBarFocus: true,
      expectedURLBarValue: "",
    },
  ];

  for (const testData of TEST_DATA) {
    await testMiddleClickOnHomeButton(testData);
  }
});

add_task(async function test_middleClickOnHomeButtonWithNewWindow() {
  await testMiddleClickOnHomeButtonWithNewWindow(false);
  await testMiddleClickOnHomeButtonWithNewWindow(true);
});

async function testMiddleClickOnTab(isMiddleMousePastePrefOn) {
  info(`Set middlemouse.paste [${isMiddleMousePastePrefOn}]`);
  Services.prefs.setBoolPref("middlemouse.paste", isMiddleMousePastePrefOn);

  info("Set initial value");
  SpecialPowers.clipboardCopyString("test\nsample");
  gURLBar.value = "";
  gURLBar.focus();

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
  info(`Set middlemouse.paste [${isMiddleMousePastePrefOn}]`);
  Services.prefs.setBoolPref("middlemouse.paste", isMiddleMousePastePrefOn);

  info("Set initial value");
  SpecialPowers.clipboardCopyString("test\nsample");
  gURLBar.value = "";
  gURLBar.focus();

  info("Middle click on the urlbar");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { button: 1 });

  if (isMiddleMousePastePrefOn) {
    Assert.equal(gURLBar.value, "test sample", "URLBar has pasted value");
  } else {
    Assert.equal(gURLBar.value, "", "URLBar has no pasted value");
  }
}

async function testMiddleClickOnHomeButton({
  isMiddleMousePastePrefOn,
  isLoadInBackground,
  startPagePref,
  expectedURLBarFocus,
  expectedURLBarValue,
}) {
  info(`middlemouse.paste [${isMiddleMousePastePrefOn}]`);
  info(`browser.startup.homepage [${startPagePref}]`);
  info(`browser.tabs.loadBookmarksInBackground [${isLoadInBackground}]`);

  info("Set initial value");
  Services.prefs.setCharPref("browser.startup.homepage", startPagePref);
  Services.prefs.setBoolPref(
    "browser.tabs.loadBookmarksInBackground",
    isLoadInBackground
  );
  Services.prefs.setBoolPref("middlemouse.paste", isMiddleMousePastePrefOn);
  SpecialPowers.clipboardCopyString("test\nsample");
  gURLBar.value = "";
  gURLBar.focus();

  info("Middle click on the home button");
  const currentTab = gBrowser.selectedTab;
  const homeButton = document.getElementById("home-button");
  EventUtils.synthesizeMouseAtCenter(homeButton, { button: 1 });

  if (!isLoadInBackground) {
    info("Wait until the a new tab is selected");
    await TestUtils.waitForCondition(() => gBrowser.selectedTab !== currentTab);
  }

  info("Wait until the focus moves");
  await TestUtils.waitForCondition(
    () =>
      (document.activeElement === gURLBar.inputField) === expectedURLBarFocus
  );

  Assert.ok(true, "The focus is correct");
  Assert.equal(gURLBar.value, expectedURLBarValue, "URLBar value is correct");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

async function testMiddleClickOnHomeButtonWithNewWindow(
  isMiddleMousePastePrefOn
) {
  info(`Set middlemouse.paste [${isMiddleMousePastePrefOn}]`);
  Services.prefs.setBoolPref("middlemouse.paste", isMiddleMousePastePrefOn);

  info("Set prefs to open in a new window");
  Services.prefs.setBoolPref("middlemouse.openNewWindow", true);
  Services.prefs.setBoolPref("browser.tabs.opentabfor.middleclick", false);

  info("Set initial value");
  SpecialPowers.clipboardCopyString("test\nsample");
  gURLBar.value = "";
  gURLBar.focus();

  info("Middle click on the home button");
  const homeButton = document.getElementById("home-button");
  const onNewWindowOpened = BrowserTestUtils.waitForNewWindow();
  EventUtils.synthesizeMouseAtCenter(homeButton, { button: 1 });

  const newWindow = await onNewWindowOpened;
  Assert.equal(newWindow.gURLBar.value, "", "URLBar value is correct");

  await BrowserTestUtils.closeWindow(newWindow);
}
