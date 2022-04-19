/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

async function setupMSExperimentWithMessage(message) {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pbNewtab",
    enabled: true,
    value: message,
  });
  Services.prefs.setStringPref(
    "browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments",
    '{"id":"messaging-experiments","enabled":true,"type":"remote-experiments","messageGroups":["pbNewtab"],"updateCycleInMs":0}'
  );
  // Reload the provider
  await ASRouter._updateMessageProviders();
  // Wait to load the messages from the messaging-experiments provider
  await ASRouter.loadMessagesFromAllProviders();

  registerCleanupFunction(async () => {
    // Reload the provider again at cleanup to remove the experiment message
    await ASRouter._updateMessageProviders();
    // Wait to load the messages from the messaging-experiments provider
    await ASRouter.loadMessagesFromAllProviders();
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments"
    );
  });

  Assert.ok(
    ASRouter.state.messages.find(m => m.id.includes(message.id)),
    "Experiment message found in ASRouter state"
  );

  return doExperimentCleanup;
}

add_setup(async function() {
  let { win } = await openAboutPrivateBrowsing();
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_experiment_messaging_system() {
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      promoEnabled: true,
      infoEnabled: true,
      promoHeader: "fluent:about-private-browsing-focus-promo-header-c",
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com/%LOCALE%",
      promoLinkUrl: "http://bar.example.com/%LOCALE%",
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  Services.telemetry.clearEvents();

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [LOCALE], async function(locale) {
    const infoBody = content.document.getElementById("info-body");
    const promoHeader = content.document.getElementById("promo-header");
    const promoLink = content.document.getElementById(
      "private-browsing-vpn-link"
    );

    // Check experiment values are rendered
    is(
      promoHeader.textContent,
      "Next-level privacy on mobile",
      "should render promo header with fluent"
    );
    is(
      infoBody.textContent,
      "You’re in a Private Window",
      "should render infoBody with fluent"
    );
    is(
      promoLink.textContent,
      "Stay private with Mozilla VPN",
      "should render promoLinkText with fluent"
    );
    is(
      content.document.querySelector(".info a").getAttribute("href"),
      "http://foo.example.com/" + locale,
      "should format the infoLinkUrl url"
    );
    is(
      content.document.querySelector(".info a").getAttribute("target"),
      "_blank",
      "should open info url in new tab"
    );
    is(
      content.document.querySelector(".promo button").getAttribute("target"),
      "_blank",
      "should open promo url in new tab"
    );
  });

  // There's something buggy here, disabling for now to prevent intermittent failures
  // until we fix it in bug 1754536.
  //
  //  TelemetryTestUtils.assertEvents(
  //    [
  //     {
  //
  //       method: "expose",
  //       extra: {
  //          featureId: "pbNewtab",
  //        },
  //      },
  //    ],
  //    { category: "normandy" }
  //  );

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_messaging_system_impressions() {
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: `PB_NEWTAB_MESSAGING_SYSTEM_${Math.random()}`,
    template: "pb_newtab",
    content: {
      promoEnabled: true,
      infoEnabled: true,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com/%LOCALE%",
      promoLinkUrl: "http://bar.example.com/%LOCALE%",
    },
    frequency: {
      lifetime: 2,
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });
  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [LOCALE], async function(locale) {
    is(
      content.document.querySelector(".promo button").getAttribute("href"),
      "http://bar.example.com/" + locale,
      "should format the promoLinkUrl url"
    );
  });

  let { win: win2, tab: tab2 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab2, [LOCALE], async function(locale) {
    is(
      content.document.querySelector(".promo button").getAttribute("href"),
      "http://bar.example.com/" + locale,
      "should format the promoLinkUrl url"
    );
  });

  let { win: win3, tab: tab3 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab3, [], async function() {
    is(
      content.document.querySelector(".promo button"),
      null,
      "should no longer render the experiment message after 2 impressions"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win3);
  await doExperimentCleanup();
});

add_task(async function test_experiment_messaging_system_dismiss() {
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: `PB_NEWTAB_MESSAGING_SYSTEM_${Math.random()}`,
    template: "pb_newtab",
    content: {
      promoEnabled: true,
      infoEnabled: true,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com/%LOCALE%",
      promoLinkUrl: "http://bar.example.com/%LOCALE%",
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [LOCALE], async function(locale) {
    is(
      content.document.querySelector(".promo button").getAttribute("href"),
      "http://bar.example.com/" + locale,
      "should format the promoLinkUrl url"
    );

    content.document.querySelector("#dismiss-btn").click();
  });

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
