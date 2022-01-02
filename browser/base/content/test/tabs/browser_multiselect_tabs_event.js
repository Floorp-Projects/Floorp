/* eslint-disable mozilla/no-arbitrary-setTimeout */

add_task(async function clickWithPrefSet() {
  let detectUnexpected = true;
  window.addEventListener("TabMultiSelect", () => {
    if (detectUnexpected) {
      ok(false, "Shouldn't get unexpected event");
    }
  });
  async function expectEvent(callback, expectedTabs) {
    let event = new Promise(resolve => {
      detectUnexpected = false;
      window.addEventListener(
        "TabMultiSelect",
        () => {
          detectUnexpected = true;
          resolve();
        },
        { once: true }
      );
    });
    await callback();
    await event;
    ok(true, "Got TabMultiSelect event");
    expectSelected(expectedTabs);
    // Await some time to ensure no additional event is triggered
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  async function expectNoEvent(callback, expectedTabs) {
    await callback();
    expectSelected(expectedTabs);
    // Await some time to ensure no event is triggered
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  function expectSelected(expected) {
    let { selectedTabs } = gBrowser;
    is(selectedTabs.length, expected.length, "Check number of selected tabs");
    for (
      let i = 0, n = Math.min(expected.length, selectedTabs.length);
      i < n;
      ++i
    ) {
      is(selectedTabs[i], expected[i], `Check the selected tab #${i + 1}`);
    }
  }

  let initialTab = gBrowser.selectedTab;
  let tab1, tab2, tab3;

  info("Expect no event when opening tabs");
  await expectNoEvent(async () => {
    tab1 = await addTab();
    tab2 = await addTab();
    tab3 = await addTab();
  }, [initialTab]);

  info("Switching tab should trigger event");
  await expectEvent(async () => {
    await BrowserTestUtils.switchTab(gBrowser, tab1);
  }, [tab1]);

  info("Multiselecting tab with Ctrl+click should trigger event");
  await expectEvent(async () => {
    await triggerClickOn(tab2, { ctrlKey: true });
  }, [tab1, tab2]);

  info("Unselecting tab with Ctrl+click should trigger event");
  await expectEvent(async () => {
    await triggerClickOn(tab2, { ctrlKey: true });
  }, [tab1]);

  info("Multiselecting tabs with Shift+click should trigger event");
  await expectEvent(async () => {
    await triggerClickOn(tab3, { shiftKey: true });
  }, [tab1, tab2, tab3]);

  info("Expect no event if multiselection doesn't change with Shift+click");
  await expectNoEvent(async () => {
    await triggerClickOn(tab3, { shiftKey: true });
  }, [tab1, tab2, tab3]);

  info(
    "Expect no event if multiselection doesn't change with Ctrl+Shift+click"
  );
  await expectNoEvent(async () => {
    await triggerClickOn(tab2, { ctrlKey: true, shiftKey: true });
  }, [tab1, tab2, tab3]);

  info(
    "Expect no event if selected tab doesn't change with gBrowser.selectedTab"
  );
  await expectNoEvent(async () => {
    gBrowser.selectedTab = tab1;
  }, [tab1, tab2, tab3]);

  info(
    "Clearing multiselection by switching tab with gBrowser.selectedTab should trigger event"
  );
  await expectEvent(async () => {
    await BrowserTestUtils.switchTab(gBrowser, () => {
      gBrowser.selectedTab = tab3;
    });
  }, [tab3]);

  info(
    "Click on the active and the only mutliselected tab should not trigger event"
  );
  await expectNoEvent(async () => {
    await triggerClickOn(tab3, {});
  }, [tab3]);

  info(
    "Expect no event if selected tab doesn't change with gBrowser.selectedTabs"
  );
  gBrowser.selectedTabs = [tab3];
  expectSelected([tab3]);

  info("Multiselecting tabs with gBrowser.selectedTabs should trigger event");
  await expectEvent(async () => {
    gBrowser.selectedTabs = [tab3, tab2, tab1];
  }, [tab1, tab2, tab3]);

  info(
    "Expect no event if multiselection doesn't change with gBrowser.selectedTabs"
  );
  await expectNoEvent(async () => {
    gBrowser.selectedTabs = [tab3, tab1, tab2];
  }, [tab1, tab2, tab3]);

  info("Switching tab with gBrowser.selectedTabs should trigger event");
  await expectEvent(async () => {
    gBrowser.selectedTabs = [tab1, tab2, tab3];
  }, [tab1, tab2, tab3]);

  info(
    "Unmultiselection tab with removeFromMultiSelectedTabs should trigger event"
  );
  await expectEvent(async () => {
    gBrowser.removeFromMultiSelectedTabs(tab3);
  }, [tab1, tab2]);

  info("Expect no event if the tab is not multiselected");
  await expectNoEvent(async () => {
    gBrowser.removeFromMultiSelectedTabs(tab3);
  }, [tab1, tab2]);

  info(
    "Clearing multiselection with clearMultiSelectedTabs should trigger event"
  );
  await expectEvent(async () => {
    gBrowser.clearMultiSelectedTabs();
  }, [tab1]);

  info("Expect no event if there is no multiselection to clear");
  await expectNoEvent(async () => {
    gBrowser.clearMultiSelectedTabs();
  }, [tab1]);

  info(
    "Expect no event if clearMultiSelectedTabs counteracts addToMultiSelectedTabs"
  );
  await expectNoEvent(async () => {
    gBrowser.addToMultiSelectedTabs(tab3);
    gBrowser.clearMultiSelectedTabs();
  }, [tab1]);

  info(
    "Multiselecting tab with gBrowser.addToMultiSelectedTabs should trigger event"
  );
  await expectEvent(async () => {
    gBrowser.addToMultiSelectedTabs(tab2);
  }, [tab1, tab2]);

  info(
    "Expect no event if addToMultiSelectedTabs counteracts clearMultiSelectedTabs"
  );
  await expectNoEvent(async () => {
    gBrowser.clearMultiSelectedTabs();
    gBrowser.addToMultiSelectedTabs(tab1);
    gBrowser.addToMultiSelectedTabs(tab2);
  }, [tab1, tab2]);

  info(
    "Expect no event if removeFromMultiSelectedTabs counteracts addToMultiSelectedTabs"
  );
  await expectNoEvent(async () => {
    gBrowser.addToMultiSelectedTabs(tab3);
    gBrowser.removeFromMultiSelectedTabs(tab3);
  }, [tab1, tab2]);

  info(
    "Expect no event if addToMultiSelectedTabs counteracts removeFromMultiSelectedTabs"
  );
  await expectNoEvent(async () => {
    gBrowser.removeFromMultiSelectedTabs(tab2);
    gBrowser.addToMultiSelectedTabs(tab2);
  }, [tab1, tab2]);

  info("Multiselection with addRangeToMultiSelectedTabs should trigger event");
  await expectEvent(async () => {
    gBrowser.addRangeToMultiSelectedTabs(tab1, tab3);
  }, [tab1, tab2, tab3]);

  info("Switching to a just multiselected tab should multiselect the old one");
  await expectEvent(async () => {
    gBrowser.clearMultiSelectedTabs();
  }, [tab1]);
  await expectEvent(async () => {
    is(tab1.multiselected, false, "tab1 is not multiselected");
    gBrowser.addToMultiSelectedTabs(tab2);
    gBrowser.lockClearMultiSelectionOnce();
    gBrowser.selectedTab = tab2;
  }, [tab1, tab2]);
  is(tab1.multiselected, true, "tab1 becomes multiselected");

  detectUnexpected = false;
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
