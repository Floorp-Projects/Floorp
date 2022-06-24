/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function synthesizeKeyAndWaitForFocus(element, keyCode, options) {
  let focused = BrowserTestUtils.waitForEvent(element, "focus");
  EventUtils.synthesizeKey(keyCode, options);
  return focused;
}

function synthesizeKeyAndWaitForTabToGetKeyboardFocus(tab, keyCode, options) {
  let focused = TestUtils.waitForCondition(() => {
    return tab.classList.contains("keyboard-focused-tab");
  }, "Waiting for tab to get keyboard focus");
  EventUtils.synthesizeKey(keyCode, options);
  return focused;
}

add_setup(async function() {
  // The DevEdition has the DevTools button in the toolbar by default. Remove it
  // to prevent branch-specific rules what button should be focused.
  CustomizableUI.removeWidgetFromArea("developer-button");

  let prevActiveElement = document.activeElement;
  registerCleanupFunction(() => {
    CustomizableUI.reset();
    prevActiveElement.focus();
  });
});

add_task(async function changeSelectionUsingKeyboard() {
  const tab1 = await addTab("http://mochi.test:8888/1");
  const tab2 = await addTab("http://mochi.test:8888/2");
  const tab3 = await addTab("http://mochi.test:8888/3");
  const tab4 = await addTab("http://mochi.test:8888/4");
  const tab5 = await addTab("http://mochi.test:8888/5");

  await BrowserTestUtils.switchTab(gBrowser, tab3);
  info("Move focus to location bar using the keyboard");
  await synthesizeKeyAndWaitForFocus(gURLBar, "l", { accelKey: true });
  is(document.activeElement, gURLBar.inputField, "urlbar should be focused");

  info("Move focus to the selected tab using the keyboard");
  let trackingProtectionIconContainer = document.querySelector(
    "#tracking-protection-icon-container"
  );
  await synthesizeKeyAndWaitForFocus(
    trackingProtectionIconContainer,
    "VK_TAB",
    { shiftKey: true }
  );
  is(
    document.activeElement,
    trackingProtectionIconContainer,
    "tracking protection icon container should be focused"
  );
  await synthesizeKeyAndWaitForFocus(
    document.getElementById("reload-button"),
    "VK_TAB",
    { shiftKey: true }
  );
  await synthesizeKeyAndWaitForFocus(
    document.getElementById("tabs-newtab-button"),
    "VK_TAB",
    { shiftKey: true }
  );
  await synthesizeKeyAndWaitForFocus(tab3, "VK_TAB", { shiftKey: true });
  is(document.activeElement, tab3, "Tab3 should be focused");

  info("Move focus to tab 1 using the keyboard");
  await synthesizeKeyAndWaitForTabToGetKeyboardFocus(tab2, "KEY_ArrowLeft", {
    accelKey: true,
  });
  await synthesizeKeyAndWaitForTabToGetKeyboardFocus(tab1, "KEY_ArrowLeft", {
    accelKey: true,
  });
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab1,
    "Tab1 should be the ariaFocusedItem"
  );

  ok(!tab1.multiselected, "Tab1 shouldn't be multiselected");
  info("Select tab1 using keyboard");
  EventUtils.synthesizeKey("VK_SPACE", { accelKey: true });
  ok(tab1.multiselected, "Tab1 should be multiselected");

  info("Move focus to tab 5 using the keyboard");
  await synthesizeKeyAndWaitForTabToGetKeyboardFocus(tab2, "KEY_ArrowRight", {
    accelKey: true,
  });
  await synthesizeKeyAndWaitForTabToGetKeyboardFocus(tab3, "KEY_ArrowRight", {
    accelKey: true,
  });
  await synthesizeKeyAndWaitForTabToGetKeyboardFocus(tab4, "KEY_ArrowRight", {
    accelKey: true,
  });
  await synthesizeKeyAndWaitForTabToGetKeyboardFocus(tab5, "KEY_ArrowRight", {
    accelKey: true,
  });

  ok(!tab5.multiselected, "Tab5 shouldn't be multiselected");
  info("Select tab5 using keyboard");
  EventUtils.synthesizeKey("VK_SPACE", { accelKey: true });
  ok(tab5.multiselected, "Tab5 should be multiselected");

  ok(
    tab1.multiselected && gBrowser._multiSelectedTabsSet.has(tab1),
    "Tab1 is (multi) selected"
  );
  ok(
    tab3.multiselected && gBrowser._multiSelectedTabsSet.has(tab3),
    "Tab3 is (multi) selected"
  );
  ok(
    tab5.multiselected && gBrowser._multiSelectedTabsSet.has(tab5),
    "Tab5 is (multi) selected"
  );
  is(gBrowser.multiSelectedTabsCount, 3, "Three tabs (multi) selected");
  is(tab3, gBrowser.selectedTab, "Tab3 is still the selected tab");

  await synthesizeKeyAndWaitForFocus(tab4, "KEY_ArrowLeft", {});
  is(
    tab4,
    gBrowser.selectedTab,
    "Tab4 is now selected tab since tab5 had keyboard focus"
  );

  is(tab4.previousElementSibling, tab3, "tab4 should be after tab3");
  is(tab4.nextElementSibling, tab5, "tab4 should be before tab5");

  let tabsReordered = BrowserTestUtils.waitForCondition(() => {
    return (
      tab4.previousElementSibling == tab2 && tab4.nextElementSibling == tab3
    );
  }, "tab4 should now be after tab2 and before tab3");
  EventUtils.synthesizeKey("KEY_ArrowLeft", { accelKey: true, shiftKey: true });
  await tabsReordered;

  is(tab4.previousElementSibling, tab2, "tab4 should be after tab2");
  is(tab4.nextElementSibling, tab3, "tab4 should be before tab3");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
  BrowserTestUtils.removeTab(tab5);
});
