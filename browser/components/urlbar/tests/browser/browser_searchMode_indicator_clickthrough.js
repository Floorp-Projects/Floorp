/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check clicking on the search mode indicator when the urlbar is not focused puts
 * focus in the urlbar.
 */

add_task(async function test() {
  // Avoid remote connections.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.suggest.enabled", false]],
  });

  await BrowserTestUtils.withNewTab("about:robots", async browser => {
    // View open, with string.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    const indicator = document.getElementById("urlbar-search-mode-indicator");
    Assert.ok(!BrowserTestUtils.is_visible(indicator));
    const indicatorCloseButton = document.getElementById(
      "urlbar-search-mode-indicator-close"
    );
    Assert.ok(!BrowserTestUtils.is_visible(indicatorCloseButton));
    const labelBox = document.getElementById("urlbar-label-box");
    Assert.ok(!BrowserTestUtils.is_visible(labelBox));

    await UrlbarTestUtils.enterSearchMode(window);
    Assert.ok(BrowserTestUtils.is_visible(indicator));
    Assert.ok(BrowserTestUtils.is_visible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.is_visible(labelBox));

    info("Blur the urlbar");
    gURLBar.blur();
    Assert.ok(BrowserTestUtils.is_visible(indicator));
    Assert.ok(BrowserTestUtils.is_visible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.is_visible(labelBox));
    Assert.notEqual(
      document.activeElement,
      gURLBar.inputField,
      "URL Bar should not be focused"
    );

    info("Focus the urlbar clicking on the indicator");
    EventUtils.synthesizeMouseAtCenter(indicator, {});
    Assert.ok(BrowserTestUtils.is_visible(indicator));
    Assert.ok(BrowserTestUtils.is_visible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.is_visible(labelBox));
    Assert.equal(
      document.activeElement,
      gURLBar.inputField,
      "URL Bar should be focused"
    );

    info("Leave search mode clicking on the close button");
    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    Assert.ok(!BrowserTestUtils.is_visible(indicator));
    Assert.ok(!BrowserTestUtils.is_visible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.is_visible(labelBox));
  });

  await BrowserTestUtils.withNewTab("about:robots", async browser => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    const indicator = document.getElementById("urlbar-search-mode-indicator");
    Assert.ok(!BrowserTestUtils.is_visible(indicator));
    const indicatorCloseButton = document.getElementById(
      "urlbar-search-mode-indicator-close"
    );
    Assert.ok(!BrowserTestUtils.is_visible(indicatorCloseButton));

    await UrlbarTestUtils.enterSearchMode(window);
    Assert.ok(BrowserTestUtils.is_visible(indicator));
    Assert.ok(BrowserTestUtils.is_visible(indicatorCloseButton));

    info("Blur the urlbar");
    gURLBar.blur();
    Assert.ok(BrowserTestUtils.is_visible(indicator));
    Assert.ok(BrowserTestUtils.is_visible(indicatorCloseButton));
    Assert.notEqual(
      document.activeElement,
      gURLBar.inputField,
      "URL Bar should not be focused"
    );

    info("Leave search mode clicking on the close button while unfocussing");
    await UrlbarTestUtils.exitSearchMode(window, {
      clickClose: true,
      waitForSearch: false,
    });
    Assert.ok(!BrowserTestUtils.is_visible(indicator));
    Assert.ok(!BrowserTestUtils.is_visible(indicatorCloseButton));
  });
});
