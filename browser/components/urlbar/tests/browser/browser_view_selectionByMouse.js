/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test selection on result view by mouse.

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.shortcuts.quickactions", true],
    ],
  });

  UrlbarTestUtils.disableResultMenuAutohide(window);

  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  UrlbarProviderQuickActions.addAction("test-addons", {
    commands: ["test-addons"],
    label: "quickactions-addons",
    onPick: () =>
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "about:about"
      ),
  });
  UrlbarProviderQuickActions.addAction("test-downloads", {
    commands: ["test-downloads"],
    label: "quickactions-downloads2",
    onPick: () =>
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "about:downloads"
      ),
  });

  registerCleanupFunction(function () {
    UrlbarProviderQuickActions.removeAction("test-addons");
    UrlbarProviderQuickActions.removeAction("test-downloads");
  });
});

add_task(async function basic() {
  const testData = [
    {
      description: "Normal result to quick action button",
      mousedown: ".urlbarView-row:nth-child(1) > .urlbarView-row-inner",
      mouseup: ".urlbarView-quickaction-button[data-key=test-downloads]",
      expected: "about:downloads",
    },
    {
      description: "Normal result to out of result",
      mousedown: ".urlbarView-row:nth-child(1) > .urlbarView-row-inner",
      mouseup: "body",
      expected: false,
    },
    {
      description: "Quick action button to normal result",
      mousedown: ".urlbarView-quickaction-button[data-key=test-addons]",
      mouseup: ".urlbarView-row:nth-child(1)",
      expected: "https://example.com/?q=test",
    },
    {
      description: "Quick action button to quick action button",
      mousedown: ".urlbarView-quickaction-button[data-key=test-addons]",
      mouseup: ".urlbarView-quickaction-button[data-key=test-downloads]",
      expected: "about:downloads",
    },
    {
      description: "Quick action button to out of result",
      mousedown: ".urlbarView-quickaction-button[data-key=test-downloads]",
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
      let [downElement, upElement] = await waitForElements([
        mousedown,
        mouseup,
      ]);

      EventUtils.synthesizeMouseAtCenter(downElement, {
        type: "mousedown",
      });
      Assert.ok(
        downElement.hasAttribute("selected"),
        "Mousedown element should be selected after mousedown"
      );

      if (upElement.tagName === "html:body") {
        // We intentionally turn off this a11y check, because the following
        // click is sent to test the selection behavior using an alternative way
        // of the urlbar dismissal, where other ways are accessible, therefore
        // this test can be ignored.
        AccessibilityUtils.setEnv({
          mustHaveAccessibleRule: false,
        });
      }
      EventUtils.synthesizeMouseAtCenter(upElement, { type: "mouseup" });
      AccessibilityUtils.resetEnv();
      Assert.ok(
        !downElement.hasAttribute("selected"),
        "Mousedown element should not be selected after mouseup"
      );
      Assert.ok(
        !upElement.hasAttribute("selected"),
        "Mouseup element should not be selected after mouseup"
      );

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
      mousedown: ".urlbarView-row:nth-child(1) > .urlbarView-row-inner",
    },
    {
      description: "Normal result to out of result",
      mousedown: ".urlbarView-row:nth-child(1) > .urlbarView-row-inner",
      expected: false,
    },
    {
      description: "Quick action button to out of browser",
      mousedown: ".urlbarView-quickaction-button[data-key=test-addons]",
    },
  ];

  for (const { description, mousedown } of testData) {
    info(description);

    await BrowserTestUtils.withNewTab("about:blank", async () => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        value: "test",
        window,
      });
      let [downElement] = await waitForElements([mousedown]);

      EventUtils.synthesizeMouseAtCenter(downElement, {
        type: "mousedown",
      });
      Assert.ok(
        downElement.hasAttribute("selected"),
        "Mousedown element should be selected after mousedown"
      );

      // Mouseup at out of browser.
      EventUtils.synthesizeMouse(document.documentElement, -1, -1, {
        type: "mouseup",
      });

      Assert.ok(
        !downElement.hasAttribute("selected"),
        "Mousedown element should not be selected after mouseup"
      );
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
        selectedElementByKey:
          "#urlbar-results .urlbarView-row > .urlbarView-row-inner[selected]",
        selectedElementAfterMouseDown:
          "#urlbar-results .urlbarView-row > .urlbarView-row-inner[selected]",
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
          "#urlbar-results .urlbarView-quickaction-button[selected]",
        selectedElementAfterMouseDown:
          "#urlbar-results .urlbarView-quickaction-button[selected]",
        actionedPage: false,
      },
    },
    {
      description: "Select normal result, then click on about:downloads",
      mousedown: ".urlbarView-quickaction-button[data-key=test-downloads]",
      mouseup: ".urlbarView-quickaction-button[data-key=test-downloads]",
      expected: {
        selectedElementByKey:
          "#urlbar-results .urlbarView-row > .urlbarView-row-inner[selected]",
        selectedElementAfterMouseDown:
          ".urlbarView-quickaction-button[data-key=test-downloads]",
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
      let [downElement, upElement] = await waitForElements([
        mousedown,
        mouseup,
      ]);

      if (arrowDown) {
        EventUtils.synthesizeKey(
          "KEY_ArrowDown",
          { repeat: arrowDown },
          window
        );
      }

      let [selectedElementByKey] = await waitForElements([
        expected.selectedElementByKey,
      ]);
      Assert.ok(
        selectedElementByKey.hasAttribute("selected"),
        "selectedElementByKey should be selected after arrow down"
      );

      EventUtils.synthesizeMouseAtCenter(downElement, {
        type: "mousedown",
      });

      if (
        expected.selectedElementByKey !== expected.selectedElementAfterMouseDown
      ) {
        let [selectedElementAfterMouseDown] = await waitForElements([
          expected.selectedElementAfterMouseDown,
        ]);
        Assert.ok(
          selectedElementAfterMouseDown.hasAttribute("selected"),
          "selectedElementAfterMouseDown should be selected after mousedown"
        );
        Assert.ok(
          !selectedElementByKey.hasAttribute("selected"),
          "selectedElementByKey should not be selected after mousedown"
        );
      }

      if (upElement.tagName === "html:body") {
        // We intentionally turn off this a11y check, because the following
        // click is sent to test the selection behavior using an alternative way
        // of the urlbar dismissal, where other ways are accessible, therefore
        // this test can be ignored.
        AccessibilityUtils.setEnv({
          mustHaveAccessibleRule: false,
        });
      }
      EventUtils.synthesizeMouseAtCenter(upElement, {
        type: "mouseup",
      });
      AccessibilityUtils.resetEnv();

      if (expected.actionedPage) {
        Assert.ok(
          !selectedElementByKey.hasAttribute("selected"),
          "selectedElementByKey should not be selected after page starts load"
        );
        await BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          false,
          expected.actionedPage
        );
        Assert.ok(true, "Expected page is opened");
      } else {
        Assert.ok(
          selectedElementByKey.hasAttribute("selected"),
          "selectedElementByKey should remain selected"
        );
      }
    });
  }
});

add_task(async function withDnsFirstForSingleWordsPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.fixup.dns_first_for_single_words", true]],
  });

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "https://example.org/",
    title: "example",
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "ex",
    window,
  });

  const details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  const target = details.element.action;
  EventUtils.synthesizeMouseAtCenter(target, { type: "mousedown" });
  const onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "https://example.org/"
  );
  EventUtils.synthesizeMouseAtCenter(target, { type: "mouseup" });
  await onLoaded;
  Assert.ok(true, "Expected page is opened");

  await PlacesUtils.bookmarks.eraseEverything();
  await SpecialPowers.popPrefEnv();
});

add_task(async function buttons() {
  let initialTabUrl = "https://example.com/initial";
  let mainResultUrl = "https://example.com/main";
  let mainResultHelpUrl = "https://example.com/help";
  let otherResultUrl = "https://example.com/other";

  let searchString = "test";

  // Add a provider with two results: The first has buttons and the second has a
  // URL that should or shouldn't become the input's value when the block button
  // in the first result is clicked, depending on the test.
  let provider = new UrlbarTestUtils.TestProvider({
    priority: Infinity,
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: mainResultUrl,
          helpUrl: mainResultHelpUrl,
          helpL10n: {
            id: "urlbar-result-menu-learn-more-about-firefox-suggest",
          },
          isBlockable: true,
          blockL10n: {
            id: "urlbar-result-menu-dismiss-firefox-suggest",
          },
        }
      ),
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: otherResultUrl,
        }
      ),
    ],
  });

  UrlbarProvidersManager.registerProvider(provider);

  let assertResultMenuOpen = () => {
    Assert.equal(
      gURLBar.view.resultMenu.state,
      "showing",
      "Result menu is showing"
    );
    EventUtils.synthesizeKey("KEY_Escape");
  };

  let testData = [
    {
      description: "Menu button to menu button",
      mousedown: ".urlbarView-row:nth-child(1) .urlbarView-button-menu",
      afterMouseupCallback: assertResultMenuOpen,
      expected: {
        mousedownSelected: false,
        topSites: {
          pageProxyState: "valid",
          value: initialTabUrl,
        },
        searchString: {
          pageProxyState: "invalid",
          value: searchString,
        },
      },
    },
    {
      description: "Row-inner to menu button",
      mousedown: ".urlbarView-row:nth-child(1) > .urlbarView-row-inner",
      mouseup: ".urlbarView-row:nth-child(1) .urlbarView-button-menu",
      afterMouseupCallback: assertResultMenuOpen,
      expected: {
        mousedownSelected: true,
        topSites: {
          pageProxyState: "valid",
          value: initialTabUrl,
        },
        searchString: {
          pageProxyState: "invalid",
          value: searchString,
        },
      },
    },
    {
      description: "Menu button to row-inner",
      mousedown: ".urlbarView-row:nth-child(1) .urlbarView-button-menu",
      mouseup: ".urlbarView-row:nth-child(1) > .urlbarView-row-inner",
      expected: {
        mousedownSelected: false,
        url: mainResultUrl,
        newTab: false,
      },
    },
  ];

  for (let showTopSites of [true, false]) {
    for (let {
      description,
      mousedown,
      mouseup,
      expected,
      afterMouseupCallback = null,
    } of testData) {
      info(`Running test with showTopSites = ${showTopSites}: ${description}`);
      mouseup ||= mousedown;

      await BrowserTestUtils.withNewTab(initialTabUrl, async () => {
        Assert.equal(
          gURLBar.getAttribute("pageproxystate"),
          "valid",
          "Sanity check: pageproxystate should be valid initially"
        );
        Assert.equal(
          gURLBar.value,
          UrlbarTestUtils.trimURL(initialTabUrl),
          "Sanity check: input.value should be the initial URL initially"
        );

        if (showTopSites) {
          // Open the view and show top sites by performing the accel+L command.
          await SimpleTest.promiseFocus(window);
          let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
          document.getElementById("Browser:OpenLocation").doCommand();
          await searchPromise;
        } else {
          // Do a search.
          await UrlbarTestUtils.promiseAutocompleteResultPopup({
            window,
            value: searchString,
          });
        }

        let [downElement, upElement] = await waitForElements([
          mousedown,
          mouseup,
        ]);

        // Mousedown and check the selection.
        EventUtils.synthesizeMouseAtCenter(downElement, {
          type: "mousedown",
        });
        if (expected.mousedownSelected) {
          Assert.ok(
            downElement.hasAttribute("selected"),
            "Mousedown element should be selected after mousedown"
          );
        } else {
          Assert.ok(
            !downElement.hasAttribute("selected"),
            "Mousedown element should not be selected after mousedown"
          );
        }

        let loadPromise;
        if (expected.url) {
          loadPromise = expected.newTab
            ? BrowserTestUtils.waitForNewTab(gBrowser, expected.url)
            : BrowserTestUtils.browserLoaded(
                gBrowser.selectedBrowser,
                null,
                expected.url
              );
        }

        // Mouseup and check the selection.
        EventUtils.synthesizeMouseAtCenter(upElement, { type: "mouseup" });
        Assert.ok(
          !downElement.hasAttribute("selected"),
          "Mousedown element should not be selected after mouseup"
        );
        Assert.ok(
          !upElement.hasAttribute("selected"),
          "Mouseup element should not be selected after mouseup"
        );

        // If we expect a URL to load, we're done since the view will close and
        // the input value will be set to the URL.
        if (loadPromise) {
          info("Waiting for URL to load: " + expected.url);
          let tab = await loadPromise;
          Assert.ok(true, "Expected URL loaded");
          if (expected.newTab) {
            BrowserTestUtils.removeTab(tab);
          }
          return;
        }

        if (afterMouseupCallback) {
          await afterMouseupCallback();
        }

        let state = showTopSites ? expected.topSites : expected.searchString;
        Assert.equal(
          gURLBar.getAttribute("pageproxystate"),
          state.pageProxyState,
          "pageproxystate should be as expected"
        );
        Assert.equal(
          gURLBar.value,
          UrlbarTestUtils.trimURL(state.value),
          "input.value should be as expected"
        );
      });
    }
  }

  UrlbarProvidersManager.unregisterProvider(provider);
});

async function waitForElements(selectors) {
  let elements;
  await BrowserTestUtils.waitForCondition(() => {
    elements = selectors.map(s => document.querySelector(s));
    return elements.every(e => e && BrowserTestUtils.isVisible(e));
  }, "Waiting for elements to become visible: " + JSON.stringify(selectors));
  return elements;
}
