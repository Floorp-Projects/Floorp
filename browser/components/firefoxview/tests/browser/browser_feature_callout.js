/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function feature_callout_renders_in_firefox_view() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", "{}"]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      ok(
        document.querySelector("#root.featureCallout"),
        "Feature Callout element exists"
      );
    }
  );
});

add_task(async function feature_callout_moves_on_screen_change() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", "{}"]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      const buttonSelector = "#root .primary";
      const calloutSelector = "#root.featureCallout";

      // Wait for callout to be rendered
      await BrowserTestUtils.waitForMutationCondition(
        document.body,
        { childList: true },
        () => document.body.querySelector(calloutSelector)
      );

      const callout = document.querySelector(calloutSelector);

      // When the callout has a style set for "top" it has
      // been positioned, which means screen content has rendered
      let startingTop;
      await BrowserTestUtils.waitForMutationCondition(
        callout,
        { attributeFilter: ["style"], attributes: true },
        () => {
          startingTop = callout.style.top;
          return callout.style.top;
        }
      );

      document.querySelector(buttonSelector).click();

      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        callout,
        { attributeFilter: ["style"], attributes: true },
        () => callout.style.top != startingTop
      );

      ok(
        startingTop !== callout.style.top,
        "Feature Callout moves to a new element when a user clicks the primary button"
      );
    }
  );
});

add_task(async function feature_callout_is_not_show_twice() {
  // Third comma-separated value of the pref is set to a string value once a user completes the tour
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.firefox-view.feature-tour",
        '{"message":"","screen":"","complete":true}',
      ],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      ok(
        !document.querySelector("#root.featureCallout"),
        "Feature Callout tour does not render if the user finished it previously"
      );
    }
  );
});
