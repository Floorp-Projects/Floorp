/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

const FXVIEW_NEXT_ENABLED_PREF = "browser.tabs.firefox-view-next";
const FXVIEW_ENABLED_PREF = "browser.tabs.firefox-view";

add_task(async function about_firefoxview_next_pref() {
  // Verify pref enables new Firefox view
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
  });
  // Verify pref enables new Firefox view even when old is disabled
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_ENABLED_PREF, false]] });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
  });
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_ENABLED_PREF, true]] });
  // Verify pref disables new Firefox view
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, false]] });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview");
  });
});

add_task(async function test_aria_roles() {
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");

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
    let recentlyClosedEmptyState = recentlyClosedComponent.emptyState;
    let descriptionEls = recentlyClosedEmptyState.descriptionEls;
    is(
      descriptionEls[1].querySelector("a").getAttribute("aria-details"),
      "card-container",
      "The link within the recently closed empty state has the expected 'aria-details' attribute."
    );

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs[slot=syncedtabs]"
    );
    let syncedTabsEmptyState = syncedTabsComponent.emptyState;
    is(
      syncedTabsEmptyState.querySelector("button").getAttribute("aria-details"),
      "empty-container",
      "The button within the synced tabs empty state has the expected 'aria-details' attribute."
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
  // Verify pref disables new Firefox view
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, false]] });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview");
  });
});

add_task(async function firefoxview_next_icon() {
  let viewButton = window.document.querySelector("#firefox-view-button");
  let style = window.getComputedStyle(viewButton);
  // Verify pref changes icon
  Services.prefs.setBoolPref("browser.tabs.firefox-view-newIcon", false);
  ok(style.listStyleImage.includes, "chrome://branding/content/about-logo.png");
  // Verify icon changes back
  Services.prefs.setBoolPref("browser.tabs.firefox-view-newIcon", true);
  ok(style.listStyleImage.includes, "chrome://browser/skin/firefox-view.svg");
});
