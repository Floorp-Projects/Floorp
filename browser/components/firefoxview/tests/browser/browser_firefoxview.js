/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function about_firefoxview_smoke_test() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    // sanity check the important regions exist on this page
    ok(document.querySelector("moz-page-nav"), "moz-page-nav element exists");
    ok(document.querySelector("named-deck"), "named-deck element exists");
  });
});

add_task(async function test_aria_roles() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview");

    is(
      document.querySelector("main").getAttribute("role"),
      "application",
      "The main element has role='application'"
    );
    // Purge session history to ensure recently closed empty state is shown
    Services.obs.notifyObservers(null, "browser:purge-session-history");
    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed[slot=recentlyclosed]"
    );
    await TestUtils.waitForCondition(
      () => recentlyClosedComponent.fullyUpdated
    );
    let recentlyClosedEmptyState = recentlyClosedComponent.emptyState;
    let descriptionEls = recentlyClosedEmptyState.descriptionEls;
    const recentlyClosedCard = SpecialPowers.wrap(
      recentlyClosedEmptyState
    ).openOrClosedShadowRoot.querySelector("card-container");
    is(
      recentlyClosedCard.getAttribute("aria-labelledby"),
      "header",
      "The recently closed empty state container has the expected 'aria-labelledby' attribute."
    );
    is(
      recentlyClosedCard.getAttribute("aria-describedby"),
      "description",
      "The recently closed empty state container has the expected 'aria-describedby' attribute."
    );
    is(
      recentlyClosedCard.getAttribute("role"),
      "group",
      "The recently closed empty state container has the expected 'role' attribute."
    );

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs[slot=syncedtabs]"
    );
    let syncedTabsEmptyState = syncedTabsComponent.emptyState;
    const syncedCard =
      SpecialPowers.wrap(
        syncedTabsEmptyState
      ).openOrClosedShadowRoot.querySelector("card-container");
    is(
      syncedCard.getAttribute("aria-labelledby"),
      "header",
      "The synced tabs empty state container has the expected 'aria-labelledby' attribute."
    );
    is(
      syncedCard.getAttribute("aria-describedby"),
      "description",
      "The synced tabs empty state container has the expected 'aria-describedby' attribute."
    );
    is(
      syncedCard.getAttribute("role"),
      "group",
      "The synced tabs empty state container has the expected 'role' attribute."
    );

    // Test keyboard navigation from card-container summary
    // elements to links/buttons in empty states
    const tab = async shiftKey => {
      info(`Tab${shiftKey ? " + Shift" : ""}`);
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey });
    };
    recentlyClosedComponent.cardEl.summaryEl.focus();
    ok(
      recentlyClosedComponent.cardEl.summaryEl.matches(":focus"),
      "Focus should be on the summary element within the recently closed card-container"
    );
    // Purge session history to ensure recently closed empty state is shown
    Services.obs.notifyObservers(null, "browser:purge-session-history");
    await TestUtils.waitForCondition(
      () => recentlyClosedComponent.fullyUpdated
    );
    await tab();
    ok(
      descriptionEls[1].querySelector("a").matches(":focus"),
      "Focus should be on the link within the recently closed empty state"
    );
    await tab();
    const shadowRoot =
      SpecialPowers.wrap(syncedTabsComponent).openOrClosedShadowRoot;
    ok(
      shadowRoot.querySelector("card-container").summaryEl.matches(":focus"),
      "Focus should be on summary element of the synced tabs card-container"
    );
    await tab();
    ok(
      syncedTabsEmptyState.querySelector("button").matches(":focus"),
      "Focus should be on button element of the synced tabs empty state"
    );
  });
});
