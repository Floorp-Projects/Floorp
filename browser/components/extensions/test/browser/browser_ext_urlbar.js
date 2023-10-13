"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

async function loadTipExtension(options = {}) {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background() {
      browser.test.onMessage.addListener(options => {
        browser.urlbar.onBehaviorRequested.addListener(query => {
          return "restricting";
        }, "test");
        browser.urlbar.onResultsRequested.addListener(query => {
          return [
            {
              type: "tip",
              source: "local",
              heuristic: true,
              payload: {
                text: "Test",
                buttonText: "OK",
                buttonUrl: options.buttonUrl,
                helpUrl: options.helpUrl,
              },
            },
          ];
        }, "test");
        browser.urlbar.onResultPicked.addListener((payload, details) => {
          browser.test.assertEq(payload.text, "Test", "payload.text");
          browser.test.assertEq(payload.buttonText, "OK", "payload.buttonText");
          browser.test.sendMessage("onResultPicked received", details);
        }, "test");
      });
    },
  });
  await ext.startup();
  ext.sendMessage(options);

  // Wait for the provider to be registered before continuing.  The provider
  // will be registered once the parent process receives the first addListener
  // call from the extension.  There's no better way to do this, unfortunately.
  // For example, if the extension sends a message to the test after it adds its
  // listeners and then we wait here for that message, there's no guarantee that
  // the addListener calls will have been received in the parent yet.
  await BrowserTestUtils.waitForCondition(
    () => UrlbarProvidersManager.getProvider("test"),
    "Waiting for provider to be registered"
  );

  Assert.ok(
    UrlbarProvidersManager.getProvider("test"),
    "Provider should have been registered"
  );
  return ext;
}

/**
 * Updates the Top Sites feed.
 *
 * @param {Function} condition
 *   A callback that returns true after Top Sites are successfully updated.
 * @param {boolean} searchShortcuts
 *   True if Top Sites search shortcuts should be enabled.
 */
async function updateTopSites(condition, searchShortcuts = false) {
  // Toggle the pref to clear the feed cache and force an update.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtabpage.activity-stream.feeds.system.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.system.topsites", true],
      [
        "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
        searchShortcuts,
      ],
    ],
  });

  // Wait for the feed to be updated.
  await TestUtils.waitForCondition(() => {
    let sites = AboutNewTab.getTopSites();
    return condition(sites);
  }, "Waiting for top sites to be updated");
}

add_setup(async function () {
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
  });
  // Set the notification timeout to a really high value to avoid intermittent
  // failures due to the mock extensions not responding in time.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.extension.timeout", 5000]],
  });
});

// Loads a tip extension without a main button URL and presses enter on the main
// button.
add_task(async function tip_onResultPicked_mainButton_noURL_enter() {
  let ext = await loadTipExtension();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
  });
  EventUtils.synthesizeKey("KEY_Enter");
  await ext.awaitMessage("onResultPicked received");
  await ext.unload();
});

// Loads a tip extension without a main button URL and clicks the main button.
add_task(async function tip_onResultPicked_mainButton_noURL_mouse() {
  let ext = await loadTipExtension();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
  });
  let mainButton = gURLBar.querySelector(".urlbarView-button-tip");
  Assert.ok(mainButton);
  EventUtils.synthesizeMouseAtCenter(mainButton, {});
  await ext.awaitMessage("onResultPicked received");
  await ext.unload();
});

// Loads a tip extension with a main button URL and presses enter on the main
// button.
add_task(async function tip_onResultPicked_mainButton_url_enter() {
  let ext = await loadTipExtension({ buttonUrl: "https://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    EventUtils.synthesizeKey("KEY_Enter");
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "https://example.com/");
  });
  await ext.unload();
});

// Loads a tip extension with a main button URL and clicks the main button.
add_task(async function tip_onResultPicked_mainButton_url_mouse() {
  let ext = await loadTipExtension({ buttonUrl: "https://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    let mainButton = gURLBar.querySelector(".urlbarView-button-tip");
    Assert.ok(mainButton);
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    EventUtils.synthesizeMouseAtCenter(mainButton, {});
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "https://example.com/");
  });
  await ext.unload();
});

// Loads a tip extension with a help button URL and presses enter on the help
// button.
add_task(async function tip_onResultPicked_helpButton_url_enter() {
  let ext = await loadTipExtension({ helpUrl: "https://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    await UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "h");
    info("Waiting for help URL to load");
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "https://example.com/");
  });
  await ext.unload();
});

// Loads a tip extension with a help button URL and clicks the help button.
add_task(async function tip_onResultPicked_helpButton_url_mouse() {
  let ext = await loadTipExtension({ helpUrl: "https://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    await UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "h", {
      openByMouse: true,
    });
    info("Waiting for help URL to load");
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "https://example.com/");
  });
  await ext.unload();
});

// Tests the search function with a non-empty string.
add_task(async function search() {
  gURLBar.blur();

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background: () => {
      browser.urlbar.search("test");
    },
  });
  await ext.startup();

  let context = await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gURLBar.value, "test");
  Assert.equal(context.searchString, "test");
  Assert.ok(gURLBar.focused);
  Assert.equal(gURLBar.getAttribute("focused"), "true");

  await UrlbarTestUtils.promisePopupClose(window);
  await ext.unload();
});

// Tests the search function with an empty string.
add_task(async function searchEmpty() {
  gURLBar.blur();

  // Searching for an empty string shows the history view, but there may be no
  // history here since other tests may have cleared it or since this test is
  // running in isolation.  We want to make sure providers are called and their
  // results are shown, so add a provider that returns a tip.
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background() {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "restricting";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(query => {
        return [
          {
            type: "tip",
            source: "local",
            heuristic: true,
            payload: {
              text: "Test",
              buttonText: "OK",
            },
          },
        ];
      }, "test");
      browser.urlbar.search("");
    },
  });
  await ext.startup();

  await BrowserTestUtils.waitForCondition(
    () => UrlbarProvidersManager.getProvider("test"),
    "Waiting for provider to be registered"
  );

  let context = await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gURLBar.value, "");
  Assert.equal(context.searchString, "");
  Assert.equal(context.results.length, 1);
  Assert.equal(context.results[0].type, UrlbarUtils.RESULT_TYPE.TIP);
  Assert.ok(gURLBar.focused);
  Assert.equal(gURLBar.getAttribute("focused"), "true");

  await UrlbarTestUtils.promisePopupClose(window);
  await ext.unload();
  await SpecialPowers.popPrefEnv();
});

// Tests the search function with `focus: false`.
add_task(async function searchFocusFalse() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesTestUtils.addVisits([
    "https://example.com/test1",
    "https://example.com/test2",
  ]);

  gURLBar.blur();

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background: () => {
      browser.urlbar.search("test", { focus: false });
    },
  });
  await ext.startup();

  let context = await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gURLBar.value, "test");
  Assert.equal(context.searchString, "test");
  Assert.ok(!gURLBar.focused);
  Assert.ok(!gURLBar.hasAttribute("focused"));

  let resultCount = UrlbarTestUtils.getResultCount(window);
  Assert.equal(resultCount, 3);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.title, "test");

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.url, "https://example.com/test2");

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.url, "https://example.com/test1");

  await UrlbarTestUtils.promisePopupClose(window);
  await ext.unload();
  await SpecialPowers.popPrefEnv();
});

// Tests the search function with `focus: false` and an empty string.
add_task(async function searchFocusFalseEmpty() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(["https://example.com/test1"]);
  }
  await updateTopSites(sites => sites.length == 1);
  gURLBar.blur();

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background: () => {
      browser.urlbar.search("", { focus: false });
    },
  });
  await ext.startup();

  let context = await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gURLBar.value, "");
  Assert.equal(context.searchString, "");
  Assert.ok(!gURLBar.focused);
  Assert.ok(!gURLBar.hasAttribute("focused"));

  let resultCount = UrlbarTestUtils.getResultCount(window);
  Assert.equal(resultCount, 1);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.url, "https://example.com/test1");

  await UrlbarTestUtils.promisePopupClose(window);
  await ext.unload();
  await SpecialPowers.popPrefEnv();
});

// Tests the focus function with select = false.
add_task(async function focusSelectFalse() {
  gURLBar.blur();
  gURLBar.value = "test";
  Assert.ok(!gURLBar.focused);
  Assert.ok(!gURLBar.hasAttribute("focused"));

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background: () => {
      browser.urlbar.focus();
    },
  });
  await ext.startup();

  await TestUtils.waitForCondition(() => gURLBar.focused);
  Assert.ok(gURLBar.focused);
  Assert.ok(gURLBar.hasAttribute("focused"));
  Assert.equal(gURLBar.selectionStart, gURLBar.selectionEnd);

  await ext.unload();
});

// Tests the focus function with select = true.
add_task(async function focusSelectTrue() {
  gURLBar.blur();
  gURLBar.value = "test";
  Assert.ok(!gURLBar.focused);
  Assert.ok(!gURLBar.hasAttribute("focused"));

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background: () => {
      browser.urlbar.focus(true);
    },
  });
  await ext.startup();

  await TestUtils.waitForCondition(() => gURLBar.focused);
  Assert.ok(gURLBar.focused);
  Assert.ok(gURLBar.hasAttribute("focused"));
  Assert.equal(gURLBar.selectionStart, 0);
  Assert.equal(gURLBar.selectionEnd, "test".length);

  await ext.unload();
});

// Tests the closeView function.
add_task(async function closeView() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
  });

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background: () => {
      browser.urlbar.closeView();
    },
  });
  await UrlbarTestUtils.promisePopupClose(window, () => ext.startup());
  await ext.unload();
});

// Tests the onEngagement events.
add_task(async function onEngagement() {
  gURLBar.blur();

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background() {
      browser.urlbar.onEngagement.addListener(state => {
        browser.test.sendMessage("onEngagement", state);
      }, "test");
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "restricting";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(query => {
        return [
          {
            type: "tip",
            source: "local",
            heuristic: true,
            payload: {
              text: "Test",
              buttonText: "OK",
            },
          },
        ];
      }, "test");
      browser.urlbar.search("");
    },
  });
  await ext.startup();

  // Start an engagement.
  let messagePromise = ext.awaitMessage("onEngagement");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
    fireInputEvent: true,
  });
  let state = await messagePromise;
  Assert.equal(state, "start");

  // Abandon the engagement.
  messagePromise = ext.awaitMessage("onEngagement");
  gURLBar.blur();
  state = await messagePromise;
  Assert.equal(state, "abandonment");

  // Start an engagement.
  messagePromise = ext.awaitMessage("onEngagement");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
    fireInputEvent: true,
  });
  state = await messagePromise;
  Assert.equal(state, "start");

  // End the engagement by pressing enter on the extension's tip result.
  messagePromise = ext.awaitMessage("onEngagement");
  EventUtils.synthesizeKey("KEY_Enter");
  state = await messagePromise;
  Assert.equal(state, "engagement");

  // We'll open about:preferences next.  Since it won't open in a new tab if the
  // current tab is blank, open a new tab now.
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Start an engagement.
    messagePromise = ext.awaitMessage("onEngagement");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
      fireInputEvent: true,
    });
    state = await messagePromise;
    Assert.equal(state, "start");

    // Press up and enter to pick the search settings button.
    messagePromise = ext.awaitMessage("onEngagement");
    EventUtils.synthesizeKey("KEY_ArrowUp");
    EventUtils.synthesizeKey("KEY_Enter");
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      "about:preferences#search"
    );
    state = await messagePromise;
    Assert.equal(state, "discard");
  });

  // Start a final engagement to make sure the previous discard didn't mess
  // anything up.
  messagePromise = ext.awaitMessage("onEngagement");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
    fireInputEvent: true,
  });
  state = await messagePromise;
  Assert.equal(state, "start");

  // End the engagement by pressing enter on the extension's tip result.
  messagePromise = ext.awaitMessage("onEngagement");
  EventUtils.synthesizeKey("KEY_Enter");
  state = await messagePromise;
  Assert.equal(state, "engagement");

  await ext.unload();
});
