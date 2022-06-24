/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

add_task(async function test_experiment_messaging_system_dismiss() {
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: `PB_NEWTAB_MESSAGING_SYSTEM_${Math.random()}`,
    template: "pb_newtab",
    content: {
      hideDefault: true,
      promoEnabled: true,
      infoEnabled: true,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com/%LOCALE%",
      promoLinkType: "link",
      promoButton: {
        action: {
          data: {
            args: "http://bar.example.com/%LOCALE%",
            where: "tabshifted",
          },
          type: "OPEN_URL",
        },
      },
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [LOCALE], async function(locale) {
    content.document.querySelector("#dismiss-btn").click();
    info("button clicked");
  });

  let telemetryEvent = await waitForTelemetryEvent("aboutprivatebrowsing");

  ok(
    telemetryEvent[2] == "click" && telemetryEvent[3] == "dismiss_button",
    "recorded the dismiss button click"
  );

  let { win: win2, tab: tab2 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab2, [], async function() {
    is(
      content.document.querySelector(".promo button"),
      null,
      "should no longer render the experiment message after dismissing"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await doExperimentCleanup();
});

add_task(async function test_experiment_messaging_show_default_on_dismiss() {
  registerCleanupFunction(() => {
    ASRouter.resetMessageState();
  });
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: `PB_NEWTAB_MESSAGING_SYSTEM_${Math.random()}`,
    template: "pb_newtab",
    content: {
      hideDefault: false,
      promoEnabled: true,
      infoEnabled: true,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com",
      promoLinkType: "link",
      promoButton: {
        action: {
          data: {
            args: "http://bar.example.com",
            where: "tabshifted",
          },
          type: "OPEN_URL",
        },
      },
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function() {
    ok(
      content.document.querySelector(".promo"),
      "should render the promo experiment message"
    );

    content.document.querySelector("#dismiss-btn").click();
    info("button clicked");
  });

  let telemetryEvent = await waitForTelemetryEvent("aboutprivatebrowsing");

  ok(
    telemetryEvent[2] == "click" && telemetryEvent[3] == "dismiss_button",
    "recorded the dismiss button click"
  );

  let { win: win2, tab: tab2 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab2, [], async function() {
    const promoHeader = content.document.getElementById("promo-header");
    ok(
      content.document.querySelector(".promo"),
      "should render the default promo message after dismissing experiment promo"
    );
    is(
      promoHeader.textContent,
      "Next-level privacy on mobile",
      "Correct default values are shown"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await doExperimentCleanup();
});
