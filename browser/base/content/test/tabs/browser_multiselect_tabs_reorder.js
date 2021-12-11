/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // Disable tab animations
  gReduceMotionOverride = true;

  let tab0 = gBrowser.selectedTab;
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();
  let tab5 = await addTab();
  let tabs = [tab0, tab1, tab2, tab3, tab4, tab5];

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab3, { ctrlKey: true });
  await triggerClickOn(tab5, { ctrlKey: true });

  is(gBrowser.selectedTab, tab1, "Tab1 is active");
  is(gBrowser.selectedTabs.length, 3, "Three selected tabs");

  for (let i of [1, 3, 5]) {
    ok(tabs[i].multiselected, "Tab" + i + " is multiselected");
  }
  for (let i of [0, 2, 4]) {
    ok(!tabs[i].multiselected, "Tab" + i + " is not multiselected");
  }
  for (let i of [0, 1, 2, 3, 4, 5]) {
    is(tabs[i]._tPos, i, "Tab" + i + " position is :" + i);
  }

  await dragAndDrop(tab3, tab4, false);

  is(gBrowser.selectedTab, tab3, "Dragged tab (tab3) is now active");
  is(gBrowser.selectedTabs.length, 3, "Three selected tabs");

  for (let i of [1, 3, 5]) {
    ok(tabs[i].multiselected, "Tab" + i + " is still multiselected");
  }
  for (let i of [0, 2, 4]) {
    ok(!tabs[i].multiselected, "Tab" + i + " is still not multiselected");
  }

  is(tab0._tPos, 0, "Tab0 position (0) doesn't change");

  // Multiselected tabs gets grouped at the start of the slide.
  is(
    tab1._tPos,
    tab3._tPos - 1,
    "Tab1 is located right at the left of the dragged tab (tab3)"
  );
  is(
    tab5._tPos,
    tab3._tPos + 1,
    "Tab5 is located right at the right of the dragged tab (tab3)"
  );
  is(tab3._tPos, 4, "Dragged tab (tab3) position is 4");

  is(tab4._tPos, 2, "Drag target (tab4) has shifted to position 2");

  for (let tab of tabs.filter(t => t != tab0)) {
    BrowserTestUtils.removeTab(tab);
  }
});
