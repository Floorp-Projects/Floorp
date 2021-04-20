/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the suppress-focus-border attribute is applied to the Urlbar
 * correctly. Its purpose is to hide the focus border after the panel is closed.
 * It also ensures we don't flash the border at the user after they click the
 * Urlbar but before we decide we're opening the view.
 */

let TEST_RESULT = new UrlbarResult(
  UrlbarUtils.RESULT_TYPE.URL,
  UrlbarUtils.RESULT_SOURCE.HISTORY,
  { url: "http://mozilla.org/" }
);

/**
 * A test provider that awaits a promise before returning results.
 */
class AwaitPromiseProvider extends UrlbarTestUtils.TestProvider {
  /**
   * @param {object} args
   *   The constructor arguments for UrlbarTestUtils.TestProvider.
   * @param {Promise} promise
   *   The promise that will be awaited before returning results.
   */
  constructor(args, promise) {
    super(args);
    this._promise = promise;
  }

  async startQuery(context, add) {
    await this._promise;
    for (let result of this._results) {
      add(this, result);
    }
  }
}

add_task(async function afterMousedown_topSites() {
  await withAwaitProvider(
    { results: [TEST_RESULT], priority: Infinity },
    getSuppressFocusPromise(),
    async () => {
      Assert.ok(
        !gURLBar.hasAttribute("suppress-focus-border"),
        "Sanity check: the Urlbar does not have the supress-focus-border attribute."
      );

      await UrlbarTestUtils.promisePopupOpen(window, () => {
        if (gURLBar.getAttribute("pageproxystate") == "invalid") {
          gURLBar.handleRevert();
        }
        EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      });
      let result = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
      Assert.ok(
        result,
        "The provider returned a result after waiting for the suppress-focus-border attribute."
      );

      await UrlbarTestUtils.promisePopupClose(window);
      Assert.ok(
        !gURLBar.hasAttribute("suppress-focus-border"),
        "The Urlbar no longer has the supress-focus-border attribute after close."
      );
    }
  );
});

add_task(async function openLocation_topSites() {
  await withAwaitProvider(
    { results: [TEST_RESULT], priority: Infinity },
    getSuppressFocusPromise(),
    async () => {
      Assert.ok(
        !gURLBar.hasAttribute("suppress-focus-border"),
        "Sanity check: the Urlbar does not have the supress-focus-border attribute."
      );

      await UrlbarTestUtils.promisePopupOpen(window, () => {
        EventUtils.synthesizeKey("l", { accelKey: true });
      });

      let result = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
      Assert.ok(
        result,
        "The provider returned a result after waiting for the suppress-focus-border attribute."
      );

      await UrlbarTestUtils.promisePopupClose(window);
      Assert.ok(
        !gURLBar.hasAttribute("suppress-focus-border"),
        "The Urlbar no longer has the supress-focus-border attribute after close."
      );
    }
  );
});

// Tests that the address bar loses the suppress-focus-border attribute if no
// results are returned by a query. This simulates the user disabling Top Sites
// then clicking the address bar.
add_task(async function afterMousedown_noTopSites() {
  await withAwaitProvider(
    // Note that the provider returns no results.
    { results: [], priority: Infinity },
    getSuppressFocusPromise(),
    async () => {
      Assert.ok(
        !gURLBar.hasAttribute("suppress-focus-border"),
        "Sanity check: the Urlbar does not have the supress-focus-border attribute."
      );

      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      // Because the panel opening may not be immediate, we must wait a bit.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 500));
      Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "The popup is not open.");

      Assert.ok(
        !gURLBar.hasAttribute("suppress-focus-border"),
        "The Urlbar no longer has the supress-focus-border attribute."
      );
    }
  );
});

// Tests that we show the focus border when new tabs are opened.
add_task(async function newTab() {
  // We have to listen for the new tab using this brute force method.
  // about:newtab is preloaded in the background. When about:newtab is opened,
  // the cached version is shown. Since the page is already loaded,
  // waitForNewTab does not detect it. It also doesn't fire the TabOpen event.
  let tabCount = gBrowser.tabs.length;
  let tabOpenPromise = TestUtils.waitForCondition(
    () =>
      gBrowser.tabs.length == tabCount + 1
        ? gBrowser.tabs[gBrowser.tabs.length - 1]
        : false,
    "Waiting for background about:newtab to open."
  );
  // Tabs opened with withNewTab don't focus the Urlbar, so we have to open one
  // manually.
  EventUtils.synthesizeKey("t", { accelKey: true });
  let tab = await tabOpenPromise;
  await BrowserTestUtils.waitForCondition(
    () => gURLBar.hasAttribute("focused"),
    "Waiting for the Urlbar to become focused."
  );
  Assert.ok(
    !gURLBar.hasAttribute(
      "suppress-focus-border",
      "The Urlbar does not have the suppress-focus-border attribute."
    )
  );
  BrowserTestUtils.removeTab(tab);
});

// Tests that we show the focus border when a new tab is opened and the address
// bar panel is already open.
add_task(async function newTab_alreadyOpen() {
  await withAwaitProvider(
    { results: [TEST_RESULT], priority: Infinity },
    getSuppressFocusPromise(),
    async () => {
      let tabCount = gBrowser.tabs.length;
      let tabOpenPromise = TestUtils.waitForCondition(
        () =>
          gBrowser.tabs.length == tabCount + 1
            ? gBrowser.tabs[gBrowser.tabs.length - 1]
            : false,
        "Waiting for background about:newtab to open."
      );
      await UrlbarTestUtils.promisePopupOpen(window, () => {
        EventUtils.synthesizeKey("l", { accelKey: true });
      });

      EventUtils.synthesizeKey("t", { accelKey: true });
      let tab = await tabOpenPromise;
      await BrowserTestUtils.waitForCondition(
        () => !UrlbarTestUtils.isPopupOpen(window),
        "Waiting for the Urlbar panel to close."
      );
      Assert.ok(
        !gURLBar.hasAttribute(
          "suppress-focus-border",
          "The Urlbar does not have the suppress-focus-border attribute."
        )
      );
      BrowserTestUtils.removeTab(tab);
    }
  );
});

add_task(async function searchTip() {
  info("Set a pref to show a search tip button.");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchTips.test.ignoreShowLimits", true]],
  });

  info("Open new tab.");
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab"
  );

  info("Click the tip button.");
  const result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  const button = result.element.row._elements.get("tipButton");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(button, {});
  });

  Assert.ok(
    !gURLBar.hasAttribute(
      "suppress-focus-border",
      "The Urlbar does not have the suppress-focus-border attribute."
    )
  );

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

function getSuppressFocusPromise(win = window) {
  return new Promise(resolve => {
    let observer = new MutationObserver(() => {
      if (
        win.gURLBar.hasAttribute("suppress-focus-border") &&
        !UrlbarTestUtils.isPopupOpen(window)
      ) {
        resolve();
        observer.disconnect();
      }
    });
    observer.observe(win.gURLBar.textbox, {
      attributes: true,
      attributeFilter: ["suppress-focus-border"],
    });
  });
}

async function withAwaitProvider(args, promise, callback) {
  let provider = new AwaitPromiseProvider(args, promise);
  UrlbarProvidersManager.registerProvider(provider);
  try {
    await callback();
  } catch (ex) {
    Cu.reportError(ex);
  } finally {
    UrlbarProvidersManager.unregisterProvider(provider);
  }
}
