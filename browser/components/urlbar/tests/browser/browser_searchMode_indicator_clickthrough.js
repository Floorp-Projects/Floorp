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
    Assert.ok(!BrowserTestUtils.isVisible(indicator));
    const indicatorCloseButton = document.getElementById(
      "urlbar-search-mode-indicator-close"
    );
    Assert.ok(!BrowserTestUtils.isVisible(indicatorCloseButton));
    const labelBox = document.getElementById("urlbar-label-box");
    Assert.ok(!BrowserTestUtils.isVisible(labelBox));

    await UrlbarTestUtils.enterSearchMode(window);
    Assert.ok(BrowserTestUtils.isVisible(indicator));
    Assert.ok(BrowserTestUtils.isVisible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.isVisible(labelBox));

    info("Blur the urlbar");
    gURLBar.blur();
    Assert.ok(BrowserTestUtils.isVisible(indicator));
    Assert.ok(BrowserTestUtils.isVisible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.isVisible(labelBox));
    Assert.notEqual(
      document.activeElement,
      gURLBar.inputField,
      "URL Bar should not be focused"
    );

    info("Focus the urlbar clicking on the indicator");
    // We intentionally turn off a11y_checks for the following click, because
    // it is send to send a focus on the URL Bar with the mouse, while other
    // ways to focus it are accessible for users of assistive technology and
    // keyboards, thus this test can be excluded from the accessibility tests.
    AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
    EventUtils.synthesizeMouseAtCenter(indicator, {});
    AccessibilityUtils.resetEnv();
    Assert.ok(BrowserTestUtils.isVisible(indicator));
    Assert.ok(BrowserTestUtils.isVisible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.isVisible(labelBox));
    Assert.equal(
      document.activeElement,
      gURLBar.inputField,
      "URL Bar should be focused"
    );

    info("Leave search mode clicking on the close button");
    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    Assert.ok(!BrowserTestUtils.isVisible(indicator));
    Assert.ok(!BrowserTestUtils.isVisible(indicatorCloseButton));
    Assert.ok(!BrowserTestUtils.isVisible(labelBox));
  });

  await BrowserTestUtils.withNewTab("about:robots", async browser => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    const indicator = document.getElementById("urlbar-search-mode-indicator");
    Assert.ok(!BrowserTestUtils.isVisible(indicator));
    const indicatorCloseButton = document.getElementById(
      "urlbar-search-mode-indicator-close"
    );
    Assert.ok(!BrowserTestUtils.isVisible(indicatorCloseButton));

    await UrlbarTestUtils.enterSearchMode(window);
    Assert.ok(BrowserTestUtils.isVisible(indicator));
    Assert.ok(BrowserTestUtils.isVisible(indicatorCloseButton));

    info("Blur the urlbar");
    gURLBar.blur();
    Assert.ok(BrowserTestUtils.isVisible(indicator));
    Assert.ok(BrowserTestUtils.isVisible(indicatorCloseButton));
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
    Assert.ok(!BrowserTestUtils.isVisible(indicator));
    Assert.ok(!BrowserTestUtils.isVisible(indicatorCloseButton));
  });
});
