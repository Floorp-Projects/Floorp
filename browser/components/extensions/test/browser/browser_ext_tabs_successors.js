/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function background(tabCount, testFn) {
  try {
    const { TAB_ID_NONE } = browser.tabs;
    const tabIds = await Promise.all(
      Array.from({ length: tabCount }, () =>
        browser.tabs.create({ url: "about:blank" }).then(t => t.id)
      )
    );

    const toTabIds = i => tabIds[i];

    const setSuccessors = mapping =>
      Promise.all(
        mapping.map((succ, i) =>
          browser.tabs.update(tabIds[i], { successorTabId: tabIds[succ] })
        )
      );

    const verifySuccessors = async function (mapping, name) {
      const promises = [],
        expected = [];
      for (let i = 0; i < mapping.length; i++) {
        if (mapping[i] !== undefined) {
          promises.push(
            browser.tabs.get(tabIds[i]).then(t => t.successorTabId)
          );
          expected.push(
            mapping[i] === TAB_ID_NONE ? TAB_ID_NONE : tabIds[mapping[i]]
          );
        }
      }
      const results = await Promise.all(promises);
      for (let i = 0; i < results.length; i++) {
        browser.test.assertEq(
          expected[i],
          results[i],
          `${name}: successorTabId of tab ${i} in mapping should be ${expected[i]}`
        );
      }
    };

    await testFn({
      TAB_ID_NONE,
      tabIds,
      toTabIds,
      setSuccessors,
      verifySuccessors,
    });

    browser.test.notifyPass("background-script");
  } catch (e) {
    browser.test.fail(`${e} :: ${e.stack}`);
    browser.test.notifyFail("background-script");
  }
}

async function runTabTest(tabCount, testFn) {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background: `(${background})(${tabCount}, ${testFn});`,
  });

  await extension.startup();
  await extension.awaitFinish("background-script");
  await extension.unload();
}

add_task(function testTabSuccessors() {
  return runTabTest(3, async function ({ TAB_ID_NONE, tabIds }) {
    const anotherWindow = await browser.windows.create({ url: "about:blank" });

    browser.test.assertEq(
      TAB_ID_NONE,
      (await browser.tabs.get(tabIds[0])).successorTabId,
      "Tabs default to an undefined successor"
    );

    // Basic getting and setting

    await browser.tabs.update(tabIds[0], { successorTabId: tabIds[1] });
    browser.test.assertEq(
      tabIds[1],
      (await browser.tabs.get(tabIds[0])).successorTabId,
      "tabs.update assigned the correct successor"
    );

    await browser.tabs.update(tabIds[0], {
      successorTabId: browser.tabs.TAB_ID_NONE,
    });
    browser.test.assertEq(
      TAB_ID_NONE,
      (await browser.tabs.get(tabIds[0])).successorTabId,
      "tabs.update cleared successor"
    );

    await browser.tabs.update(tabIds[0], { successorTabId: tabIds[1] });
    await browser.tabs.update(tabIds[0], { successorTabId: tabIds[0] });
    browser.test.assertEq(
      TAB_ID_NONE,
      (await browser.tabs.get(tabIds[0])).successorTabId,
      "Setting a tab as its own successor clears the successor instead"
    );

    // Validation tests

    await browser.test.assertRejects(
      browser.tabs.update(tabIds[0], { successorTabId: 1e8 }),
      /Invalid successorTabId/,
      "tabs.update should throw with an invalid successor tab ID"
    );

    await browser.test.assertRejects(
      browser.tabs.update(tabIds[0], {
        successorTabId: anotherWindow.tabs[0].id,
      }),
      /Successor tab must be in the same window as the tab being updated/,
      "tabs.update should throw with a successor tab ID from another window"
    );

    // Make sure the successor is truly being assigned

    await browser.tabs.update(tabIds[0], {
      successorTabId: tabIds[2],
      active: true,
    });
    await browser.tabs.remove(tabIds[0]);
    browser.test.assertEq(
      tabIds[2],
      (await browser.tabs.query({ active: true }))[0].id
    );

    return browser.tabs.remove([
      tabIds[1],
      tabIds[2],
      anotherWindow.tabs[0].id,
    ]);
  });
});

add_task(function testMoveInSuccession_appendFalse() {
  return runTabTest(
    8,
    async function ({
      TAB_ID_NONE,
      tabIds,
      toTabIds,
      setSuccessors,
      verifySuccessors,
    }) {
      await browser.tabs.moveInSuccession([1, 0].map(toTabIds), tabIds[0]);
      await verifySuccessors([TAB_ID_NONE, 0], "scenario 1");

      await browser.tabs.moveInSuccession(
        [0, 1, 2, 3].map(toTabIds),
        tabIds[0]
      );
      await verifySuccessors([1, 2, 3, 0], "scenario 2");

      await browser.tabs.moveInSuccession([1, 0].map(toTabIds), tabIds[0]);
      await verifySuccessors(
        [TAB_ID_NONE, 0],
        "scenario 1 after tab 0 has a successor"
      );

      await browser.tabs.update(tabIds[7], { successorTabId: tabIds[0] });
      await browser.tabs.moveInSuccession([4, 5, 6, 7].map(toTabIds));
      await verifySuccessors(
        new Array(4).concat([5, 6, 7, TAB_ID_NONE]),
        "scenario 4"
      );

      await setSuccessors([7, 2, 3, 4, 3, 6, 7, 5]);
      await browser.tabs.moveInSuccession(
        [4, 6, 3, 2].map(toTabIds),
        tabIds[7]
      );
      await verifySuccessors([7, TAB_ID_NONE, 7, 2, 6, 7, 3, 5], "scenario 5");

      await setSuccessors([7, 2, 3, 4, 3, 6, 7, 5]);
      await browser.tabs.moveInSuccession(
        [4, 6, 3, 2].map(toTabIds),
        tabIds[7],
        {
          insert: true,
        }
      );
      await verifySuccessors(
        [4, TAB_ID_NONE, 7, 2, 6, 4, 3, 5],
        "insert = true"
      );

      await setSuccessors([1, 2, 3, 4, 0]);
      await browser.tabs.moveInSuccession([3, 1, 2].map(toTabIds), tabIds[0], {
        insert: true,
      });
      await verifySuccessors([4, 2, 0, 1, 3], "insert = true, part 2");

      await browser.tabs.moveInSuccession([
        tabIds[0],
        tabIds[1],
        1e8,
        tabIds[2],
      ]);
      await verifySuccessors([1, 2, TAB_ID_NONE], "unknown tab ID");

      browser.test.assertTrue(
        await browser.tabs.moveInSuccession([1e8]).then(
          () => true,
          () => false
        ),
        "When all tab IDs are unknown, tabs.moveInSuccession should not throw"
      );

      // Validation tests

      await browser.test.assertRejects(
        browser.tabs.moveInSuccession([tabIds[0], tabIds[1], tabIds[0]]),
        /IDs must not occur more than once in tabIds/,
        "tabs.moveInSuccession should throw when a tab is referenced more than once in tabIds"
      );

      await browser.test.assertRejects(
        browser.tabs.moveInSuccession([tabIds[0], tabIds[1]], tabIds[0], {
          insert: true,
        }),
        /Value of tabId must not occur in tabIds if append or insert is true/,
        "tabs.moveInSuccession should throw when tabId occurs in tabIds and insert is true"
      );

      return browser.tabs.remove(tabIds);
    }
  );
});

add_task(function testMoveInSuccession_appendTrue() {
  return runTabTest(
    8,
    async function ({
      TAB_ID_NONE,
      tabIds,
      toTabIds,
      setSuccessors,
      verifySuccessors,
    }) {
      await browser.tabs.moveInSuccession([1].map(toTabIds), tabIds[0], {
        append: true,
      });
      await verifySuccessors([1, TAB_ID_NONE], "scenario 1");

      await browser.tabs.update(tabIds[3], { successorTabId: tabIds[4] });
      await browser.tabs.moveInSuccession([1, 2, 3].map(toTabIds), tabIds[0], {
        append: true,
      });
      await verifySuccessors([1, 2, 3, TAB_ID_NONE], "scenario 2");

      await browser.tabs.update(tabIds[0], { successorTabId: tabIds[1] });
      await browser.tabs.moveInSuccession([1e8], tabIds[0], { append: true });
      browser.test.assertEq(
        TAB_ID_NONE,
        (await browser.tabs.get(tabIds[0])).successorTabId,
        "If no tabs get appended after the reference tab, it should lose its successor"
      );

      await setSuccessors([7, 2, 3, 4, 3, 6, 7, 5]);
      await browser.tabs.moveInSuccession(
        [4, 6, 3, 2].map(toTabIds),
        tabIds[7],
        {
          append: true,
        }
      );
      await verifySuccessors(
        [7, TAB_ID_NONE, TAB_ID_NONE, 2, 6, 7, 3, 4],
        "scenario 3"
      );

      await setSuccessors([7, 2, 3, 4, 3, 6, 7, 5]);
      await browser.tabs.moveInSuccession(
        [4, 6, 3, 2].map(toTabIds),
        tabIds[7],
        {
          append: true,
          insert: true,
        }
      );
      await verifySuccessors(
        [7, TAB_ID_NONE, 5, 2, 6, 7, 3, 4],
        "insert = true"
      );

      await browser.tabs.moveInSuccession([0, 4].map(toTabIds), tabIds[7], {
        append: true,
        insert: true,
      });
      await verifySuccessors(
        [4, undefined, undefined, undefined, 6, undefined, undefined, 0],
        "insert = true, part 2"
      );

      await setSuccessors([1, 2, 3, 4, 0]);
      await browser.tabs.moveInSuccession([3, 1, 2].map(toTabIds), tabIds[0], {
        append: true,
        insert: true,
      });
      await verifySuccessors([3, 2, 4, 1, 0], "insert = true, part 3");

      await browser.tabs.update(tabIds[0], { successorTabId: tabIds[1] });
      await browser.tabs.moveInSuccession([1e8], tabIds[0], {
        append: true,
        insert: true,
      });
      browser.test.assertEq(
        tabIds[1],
        (await browser.tabs.get(tabIds[0])).successorTabId,
        "If no tabs get inserted after the reference tab, it should keep its successor"
      );

      // Validation tests

      await browser.test.assertRejects(
        browser.tabs.moveInSuccession([tabIds[0], tabIds[1]], tabIds[0], {
          append: true,
        }),
        /Value of tabId must not occur in tabIds if append or insert is true/,
        "tabs.moveInSuccession should throw when tabId occurs in tabIds and insert is true"
      );

      return browser.tabs.remove(tabIds);
    }
  );
});

add_task(function testMoveInSuccession_ignoreTabsInOtherWindows() {
  return runTabTest(
    2,
    async function ({
      TAB_ID_NONE,
      tabIds,
      toTabIds,
      setSuccessors,
      verifySuccessors,
    }) {
      const anotherWindow = await browser.windows.create({
        url: Array.from({ length: 3 }, () => "about:blank"),
      });
      tabIds.push(...anotherWindow.tabs.map(t => t.id));

      await setSuccessors([1, 0, 3, 4, 2]);
      await browser.tabs.moveInSuccession([1, 3, 2].map(toTabIds), tabIds[4]);
      await verifySuccessors(
        [1, 0, 4, 2, TAB_ID_NONE],
        "first tab in another window"
      );

      await setSuccessors([1, 0, 3, 4, 2]);
      await browser.tabs.moveInSuccession([3, 1, 2].map(toTabIds), tabIds[4]);
      await verifySuccessors(
        [1, 0, 4, 2, TAB_ID_NONE],
        "middle tab in another window"
      );

      await setSuccessors([1, 0, 3, 4, 2]);
      await browser.tabs.moveInSuccession([3, 1, 2].map(toTabIds));
      await verifySuccessors(
        [1, 0, TAB_ID_NONE, 2, TAB_ID_NONE],
        "using the first tab to determine the window"
      );

      await setSuccessors([1, 0, 3, 4, 2]);
      await browser.tabs.moveInSuccession([1, 3, 2].map(toTabIds), tabIds[4], {
        append: true,
      });
      await verifySuccessors(
        [1, 0, TAB_ID_NONE, 2, 3],
        "first tab in another window, appending"
      );

      await setSuccessors([1, 0, 3, 4, 2]);
      await browser.tabs.moveInSuccession([3, 1, 2].map(toTabIds), tabIds[4], {
        append: true,
      });
      await verifySuccessors(
        [1, 0, TAB_ID_NONE, 2, 3],
        "middle tab in another window, appending"
      );

      return browser.tabs.remove(tabIds);
    }
  );
});
