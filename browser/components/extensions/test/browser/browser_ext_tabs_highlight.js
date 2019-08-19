/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global gBrowser */
"use strict";

add_task(async function test_highlighted() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.multiselect", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function() {
      async function testHighlighted(activeIndex, highlightedIndices) {
        let tabs = await browser.tabs.query({ currentWindow: true });
        for (let { index, active, highlighted } of tabs) {
          browser.test.assertEq(
            index == activeIndex,
            active,
            "Check Tab.active: " + index
          );
          let expected =
            highlightedIndices.includes(index) || index == activeIndex;
          browser.test.assertEq(
            expected,
            highlighted,
            "Check Tab.highlighted: " + index
          );
        }
        let highlightedTabs = await browser.tabs.query({
          currentWindow: true,
          highlighted: true,
        });
        browser.test.assertEq(
          highlightedIndices
            .concat(activeIndex)
            .sort((a, b) => a - b)
            .join(),
          highlightedTabs.map(tab => tab.index).join(),
          "Check tabs.query with highlighted:true provides the expected tabs"
        );
      }

      browser.test.log(
        "Check that last tab is active, and no other is highlighted"
      );
      await testHighlighted(2, []);

      browser.test.log("Highlight first and second tabs");
      await browser.tabs.highlight({ tabs: [0, 1] });
      await testHighlighted(0, [1]);

      browser.test.log("Highlight second and first tabs");
      await browser.tabs.highlight({ tabs: [1, 0] });
      await testHighlighted(1, [0]);

      browser.test.log("Test that highlight fails for invalid data");
      await browser.test.assertRejects(
        browser.tabs.highlight({ tabs: [] }),
        /No highlighted tab/,
        "Attempt to highlight no tab should throw"
      );
      await browser.test.assertRejects(
        browser.tabs.highlight({ windowId: 999999999, tabs: 0 }),
        /Invalid window ID: 999999999/,
        "Attempt to highlight tabs in invalid window should throw"
      );
      await browser.test.assertRejects(
        browser.tabs.highlight({ tabs: 999999999 }),
        /No tab at index: 999999999/,
        "Attempt to highlight invalid tab index should throw"
      );
      await browser.test.assertRejects(
        browser.tabs.highlight({ tabs: [2, 999999999] }),
        /No tab at index: 999999999/,
        "Attempt to highlight invalid tab index should throw"
      );

      browser.test.log(
        "Highlighted tabs shouldn't be affected by failures above"
      );
      await testHighlighted(1, [0]);

      browser.test.log("Highlight last tab");
      let window = await browser.tabs.highlight({ tabs: 2 });
      await testHighlighted(2, []);

      browser.test.assertEq(
        3,
        window.tabs.length,
        "Returned window should be populated"
      );

      window = await browser.tabs.highlight({ tabs: 2, populate: false });
      browser.test.assertFalse(
        "tabs" in window,
        "Returned window shouldn't be populated"
      );

      browser.test.notifyPass("test-finished");
    },
  });

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  await extension.startup();
  await extension.awaitFinish("test-finished");
  await extension.unload();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
