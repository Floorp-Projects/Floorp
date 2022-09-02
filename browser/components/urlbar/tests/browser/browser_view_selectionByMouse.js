/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test selection on result view by mouse.

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.shortcuts.quickactions", true],
    ],
  });

  await SearchTestUtils.installSearchExtension();
  const defaultEngine = Services.search.getEngineByName("Example");
  const oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(defaultEngine);

  UrlbarProviderQuickActions.addAction("test-addons", {
    commands: ["test-addons"],
    label: "quickactions-addons",
    onPick: () =>
      BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:about"),
  });
  UrlbarProviderQuickActions.addAction("test-downloads", {
    commands: ["test-downloads"],
    label: "quickactions-downloads",
    onPick: () =>
      BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:downloads"),
  });

  registerCleanupFunction(function() {
    Services.search.setDefault(oldDefaultEngine);
    UrlbarProviderQuickActions.removeAction("test-addons");
    UrlbarProviderQuickActions.removeAction("test-downloads");
  });
});

add_task(async function basic() {
  const testData = [
    {
      description: "Normal result to quick action button",
      mousedown: ".urlbarView-row:nth-child(1)",
      mouseup: ".urlbarView-quickaction-row[data-key=test-downloads]",
      expected: "about:downloads",
    },
    {
      description: "Normal result to out of result",
      mousedown: ".urlbarView-row:nth-child(1)",
      mouseup: "body",
      expected: false,
    },
    {
      description: "Quick action button to normal result",
      mousedown: ".urlbarView-quickaction-row[data-key=test-addons]",
      mouseup: ".urlbarView-row:nth-child(1)",
      expected: "https://example.com/?q=test",
    },
    {
      description: "Quick action button to quick action button",
      mousedown: ".urlbarView-quickaction-row[data-key=test-addons]",
      mouseup: ".urlbarView-quickaction-row[data-key=test-downloads]",
      expected: "about:downloads",
    },
    {
      description: "Quick action button to out of result",
      mousedown: ".urlbarView-quickaction-row[data-key=test-downloads]",
      mouseup: "body",
      expected: false,
    },
  ];

  for (const { description, mousedown, mouseup, expected } of testData) {
    info(description);

    await BrowserTestUtils.withNewTab("about:blank", async () => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        value: "test",
        window,
      });
      await UrlbarTestUtils.promiseSearchComplete(window);
      await BrowserTestUtils.waitForCondition(
        () =>
          BrowserTestUtils.is_visible(document.querySelector(mousedown)) &&
          BrowserTestUtils.is_visible(document.querySelector(mouseup))
      );

      const mousedownElement = document.querySelector(mousedown);
      EventUtils.synthesizeMouseAtCenter(mousedownElement, {
        type: "mousedown",
      });
      await BrowserTestUtils.waitForCondition(() =>
        mousedownElement.hasAttribute("selected")
      );
      Assert.ok(true, "Element should be selected");

      const mouseupElement = document.querySelector(mouseup);
      EventUtils.synthesizeMouseAtCenter(mouseupElement, { type: "mouseup" });

      await BrowserTestUtils.waitForCondition(
        () => !mousedownElement.hasAttribute("selected")
      );
      Assert.ok(true, "Selected element should be unselected");

      if (expected) {
        await BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          false,
          expected
        );
        Assert.ok(true, "Expected page is opened");
      }
    });
  }
});

add_task(async function outOfBrowser() {
  const testData = [
    {
      description: "Normal result to out of browser",
      mousedown: ".urlbarView-row:nth-child(1)",
    },
    {
      description: "Normal result to out of result",
      mousedown: ".urlbarView-row:nth-child(1)",
      expected: false,
    },
    {
      description: "Quick action button to out of browser",
      mousedown: ".urlbarView-quickaction-row[data-key=test-addons]",
    },
  ];

  for (const { description, mousedown } of testData) {
    info(description);

    await BrowserTestUtils.withNewTab("about:blank", async () => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        value: "test",
        window,
      });
      await UrlbarTestUtils.promiseSearchComplete(window);
      await BrowserTestUtils.waitForCondition(() =>
        BrowserTestUtils.is_visible(document.querySelector(mousedown))
      );

      const mousedownElement = document.querySelector(mousedown);
      EventUtils.synthesizeMouseAtCenter(mousedownElement, {
        type: "mousedown",
      });
      await BrowserTestUtils.waitForCondition(() =>
        mousedownElement.hasAttribute("selected")
      );
      Assert.ok(true, "Element should be selected");

      // Mouseup at out of browser.
      EventUtils.synthesizeMouse(document.documentElement, -1, -1, {
        type: "mouseup",
      });

      await BrowserTestUtils.waitForCondition(
        () => !mousedownElement.hasAttribute("selected")
      );
      Assert.ok(true, "Selected element should be unselected");
    });
  }
});

add_task(async function withSelectionByKeyboard() {
  const testData = [
    {
      description: "Select normal result, then click on out of result",
      mousedown: "body",
      mouseup: "body",
      expected: {
        selectedElementByKey: "#urlbar-results .urlbarView-row[selected]",
        selectedElementAfterMouseDown:
          "#urlbar-results .urlbarView-row[selected]",
        actionedPage: false,
      },
    },
    {
      description: "Select quick action button, then click on out of result",
      arrowDown: 1,
      mousedown: "body",
      mouseup: "body",
      expected: {
        selectedElementByKey:
          "#urlbar-results .urlbarView-quickaction-row[selected]",
        selectedElementAfterMouseDown:
          "#urlbar-results .urlbarView-quickaction-row[selected]",
        actionedPage: false,
      },
    },
    {
      description: "Select normal result, then click on about:downloads",
      mousedown: ".urlbarView-quickaction-row[data-key=test-downloads]",
      mouseup: ".urlbarView-quickaction-row[data-key=test-downloads]",
      expected: {
        selectedElementByKey: "#urlbar-results .urlbarView-row[selected]",
        selectedElementAfterMouseDown:
          ".urlbarView-quickaction-row[data-key=test-downloads]",
        actionedPage: "about:downloads",
      },
    },
  ];

  for (const {
    description,
    arrowDown,
    mousedown,
    mouseup,
    expected,
  } of testData) {
    info(description);

    await BrowserTestUtils.withNewTab("about:blank", async () => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        value: "test",
        window,
      });
      await UrlbarTestUtils.promiseSearchComplete(window);
      await BrowserTestUtils.waitForCondition(
        () =>
          BrowserTestUtils.is_visible(document.querySelector(mousedown)) &&
          BrowserTestUtils.is_visible(document.querySelector(mouseup))
      );

      if (arrowDown) {
        EventUtils.synthesizeKey(
          "KEY_ArrowDown",
          { repeat: arrowDown },
          window
        );
      }

      await BrowserTestUtils.waitForCondition(() =>
        document.querySelector(expected.selectedElementByKey)
      );
      Assert.ok(true, "Expected element is selected");

      const selectedElementByKey = document.querySelector(
        expected.selectedElementByKey
      );
      EventUtils.synthesizeMouseAtCenter(document.querySelector(mousedown), {
        type: "mousedown",
      });

      if (
        expected.selectedElementByKey !== expected.selectedElementAfterMouseDown
      ) {
        await BrowserTestUtils.waitForCondition(() =>
          BrowserTestUtils.is_visible(
            document.querySelector(expected.selectedElementAfterMouseDown)
          )
        );
        Assert.ok(true, "Selected element is changed");
        Assert.ok(
          !selectedElementByKey.hasAttribute("selected"),
          "Selected element by key is unselected"
        );
      }

      EventUtils.synthesizeMouseAtCenter(document.querySelector(mouseup), {
        type: "mouseup",
      });

      if (expected.actionedPage) {
        await BrowserTestUtils.waitForCondition(
          () => !selectedElementByKey.hasAttribute("selected")
        );
        Assert.ok(true, "Selected element by key should be unselected");
        await BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          false,
          expected.actionedPage
        );
        Assert.ok(true, "Expected page is opened");
      } else {
        Assert.ok(
          selectedElementByKey.hasAttribute("selected"),
          "Selected element by key is still selected"
        );
      }
    });
  }
});
