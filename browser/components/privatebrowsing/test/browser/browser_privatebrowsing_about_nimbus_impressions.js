/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

/* Tests that use TelemetryTestUtils.assertEvents (at the very least, those with
 * `{ process: "content" }`) seem to be super flaky and intermittent-prone when they
 * share a file with other telemetry tests, so each one gets its own file.
 */

add_task(async function test_experiment_messaging_system_impressions() {
  registerCleanupFunction(() => {
    ASRouter.resetMessageState();
  });
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let experimentId = `pb_newtab_${Math.random()}`;

  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: experimentId,
    template: "pb_newtab",
    content: {
      hideDefault: true,
      promoEnabled: true,
      infoEnabled: true,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com/%LOCALE%",
      promoButton: {
        action: {
          data: {
            args: "https://bar.example.com/%LOCALE%",
            where: "tabshifted",
          },
          type: "OPEN_URL",
        },
      },
    },
    frequency: {
      lifetime: 2,
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  Services.telemetry.clearEvents();

  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [LOCALE], async function(locale) {
    is(
      content.document
        .querySelector(".promo button")
        .classList.contains("primary"),
      true,
      "should render the promo button as a button"
    );
  });

  let event = await waitForTelemetryEvent("normandy", experimentId);

  ok(
    event[1] == "normandy" &&
      event[2] == "expose" &&
      event[3] == "nimbus_experiment" &&
      event[4].includes(experimentId) &&
      event[5].featureId == "pbNewtab",
    "recorded telemetry for expose"
  );

  Services.telemetry.clearEvents();

  let { win: win2, tab: tab2 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab2, [LOCALE], async function(locale) {
    is(
      content.document
        .querySelector(".promo button")
        .classList.contains("primary"),
      true,
      "should render the promo button as a button"
    );
  });

  let event2 = await waitForTelemetryEvent("normandy", experimentId);

  ok(
    event2[1] == "normandy" &&
      event2[2] == "expose" &&
      event2[3] == "nimbus_experiment" &&
      event2[4].includes(experimentId) &&
      event2[5].featureId == "pbNewtab",
    "recorded telemetry for expose"
  );

  Services.telemetry.clearEvents();

  let { win: win3, tab: tab3 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab3, [], async function() {
    is(
      content.document.querySelector(".promo button"),
      null,
      "should no longer render the experiment message after 2 impressions"
    );
  });

  let event3 = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  ).content;

  Assert.equal(!!event3, false, "Should not have promo expose");

  Services.telemetry.clearEvents();

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win3);
  await doExperimentCleanup();
});
