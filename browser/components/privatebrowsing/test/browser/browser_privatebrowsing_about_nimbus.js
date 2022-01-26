/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);

/**
 * These tests ensure that the experiment and remote default capabilities
 * for the "privatebrowsing" feature are working as expected.
 */

async function openTabAndWaitForRender() {
  let { win, tab } = await openAboutPrivateBrowsing();
  await SpecialPowers.spawn(tab, [], async function() {
    // Wait for render to complete
    await ContentTaskUtils.waitForCondition(() =>
      content.document.documentElement.hasAttribute(
        "PrivateBrowsingRenderComplete"
      )
    );
  });
  return { win, tab };
}

function waitForTelemetryEvent(category) {
  info("waiting for telemetry event");
  return TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    ).content;
    if (!events) {
      return null;
    }
    events = events.filter(e => e[1] == category);
    if (events.length) {
      return events[0];
    }
    return null;
  }, "waiting for telemetry event");
}

add_task(async function test_experiment_plain_text() {
  const defaultMessageContent = (await PanelTestProvider.getMessages()).find(
    m => m.template === "pb_newtab"
  ).content;
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      ...defaultMessageContent,
      infoTitle: "Hello world",
      infoTitleEnabled: true,
      infoBody: "This is some text",
      infoLinkText: "This is a link",
      infoIcon: "chrome://branding/content/about-logo.png",
      promoTitle: "Promo title",
      promoLinkText: "Promo link",
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    const infoContainer = content.document.querySelector(".info");
    const infoTitle = content.document.getElementById("info-title");
    const infoBody = content.document.getElementById("info-body");
    const infoLink = content.document.getElementById("private-browsing-myths");
    const promoText = content.document.getElementById(
      "private-browsing-vpn-text"
    );
    const promoLink = content.document.getElementById(
      "private-browsing-vpn-link"
    );

    // Check experiment values are rendered
    ok(infoContainer, ".info container should exist");
    ok(
      infoContainer.style.backgroundImage.includes(
        "chrome://branding/content/about-logo.png"
      ),
      "should render icon"
    );
    is(infoTitle.textContent, "Hello world", "should render infoTitle");
    is(infoBody.textContent, "This is some text", "should render infoBody");
    is(infoLink.textContent, "This is a link", "should render infoLink");
    is(promoText.textContent, "Promo title", "should render promoTitle");
    is(promoLink.textContent, "Promo link", "should render promoLinkText");
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_fluent() {
  const defaultMessageContent = (await PanelTestProvider.getMessages()).find(
    m => m.template === "pb_newtab"
  ).content;
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      ...defaultMessageContent,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    const infoBody = content.document.getElementById("info-body");
    const promoLink = content.document.getElementById(
      "private-browsing-vpn-link"
    );

    // Check experiment values are rendered
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
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_info_disabled() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      infoEnabled: false,
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    is(
      content.document.querySelector(".info"),
      undefined,
      "should remove .info element"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_promo_disabled() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      promoEnabled: false,
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    is(
      content.document.querySelector(".promo"),
      undefined,
      "should remove .promo element"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_format_urls() {
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      infoEnabled: true,
      promoEnabled: true,
      infoLinkUrl: "http://foo.mozilla.com/%LOCALE%",
      promoLinkUrl: "http://bar.mozilla.com/%LOCALE%",
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [LOCALE], async function(locale) {
    is(
      content.document.querySelector(".info a").getAttribute("href"),
      "http://foo.mozilla.com/" + locale,
      "should format the infoLinkUrl url"
    );
    is(
      content.document.querySelector(".promo a").getAttribute("href"),
      "http://bar.mozilla.com/" + locale,
      "should format the promoLinkUrl url"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_click_info_telemetry() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      infoLinkUrl: "http://example.com",
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  Services.telemetry.clearEvents();

  await SpecialPowers.spawn(tab, [], () => {
    const el = content.document.querySelector(".info a");
    el.click();
  });

  let event = await waitForTelemetryEvent("aboutprivatebrowsing");

  ok(
    event[2] == "click" && event[3] == "info_link",
    "recorded telemetry for info link"
  );

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_click_promo_telemetry() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      promoEnabled: true,
      promoLinkUrl: "http://example.com",
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  Services.telemetry.clearEvents();

  await SpecialPowers.spawn(tab, [], () => {
    const el = content.document.querySelector(".promo a");
    el.click();
  });

  let event = await waitForTelemetryEvent("aboutprivatebrowsing");

  ok(
    event[2] == "click" && event[3] == "promo_link",
    "recorded telemetry for promo link"
  );

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_bottom_promo() {
  const defaultMessageContent = (await PanelTestProvider.getMessages()).find(
    m => m.template === "pb_newtab"
  ).content;
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    value: {
      ...defaultMessageContent,
      enabled: true,
      promoLinkType: "button",
      promoSectionStyle: "bottom",
      promoHeader: "Need more privacy?",
      infoTitleEnabled: true,
      promoTitleEnabled: false,
      promoImageLarge: "",
      promoImageSmall: "chrome://browser/content/assets/vpn-logo.svg",
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    is(
      content.document
        .querySelector(".promo-cta .button")
        .classList.contains("button"),
      true,
      "Should have a button CTA"
    );
    is(
      content.document.querySelector(".promo-image-small img").src,
      "chrome://browser/content/assets/vpn-logo.svg",
      "Should have logo image"
    );
    ok(
      content.document.querySelector(".promo.bottom"),
      "Should have .bottom for the promo section"
    );
    ok(
      !content.document.querySelector("#private-browsing-vpn-text"),
      "Should not render promo title if promoTitleEnabled is true"
    );
    ok(
      content.document.querySelector("#info-title"),
      "Should render info title if infoTitleEnabled is true"
    );
    ok(
      !content.document.querySelector("#private-browsing-vpn-text"),
      "Should not render promo title if promoTitleEnabled is false"
    );
  });

  await BrowserTestUtils.closeWindow(win);

  await doExperimentCleanup();
});

add_task(async function test_experiment_below_search_promo() {
  const defaultMessageContent = (await PanelTestProvider.getMessages()).find(
    m => m.template === "pb_newtab"
  ).content;
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    value: {
      ...defaultMessageContent,
      enabled: true,
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoHeader: "Need more privacy?",
      promoTitle:
        "Mozilla VPN. Security, reliability and speed — on every device,  anywhere you go.",
      promoImageLarge: "chrome://browser/content/assets/moz-vpn.svg",
      promoImageSmall: "chrome://browser/content/assets/vpn-logo.svg",
      infoTitleEnabled: false,
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    is(
      content.document
        .querySelector(".promo-cta .button")
        .classList.contains("button"),
      true,
      "Should have a button CTA"
    );
    is(
      content.document.querySelector(".promo-image-small img").src,
      "chrome://browser/content/assets/vpn-logo.svg",
      "Should have logo image"
    );
    is(
      content.document.querySelector(".promo-image-large img").src,
      "chrome://browser/content/assets/moz-vpn.svg",
      "Should have a product image"
    );
    ok(
      content.document.querySelector(".promo.below-search"),
      "Should have .below-search for the promo section"
    );
    ok(
      !content.document.querySelector("#info-title"),
      "Should not render info title if infoTitleEnabled is false"
    );
    ok(
      content.document.querySelector("#private-browsing-vpn-text"),
      "Should render promo title if promoTitleEnabled is true"
    );
  });

  await BrowserTestUtils.closeWindow(win);

  await doExperimentCleanup();
});

add_task(async function test_experiment_top_promo() {
  const defaultMessageContent = (await PanelTestProvider.getMessages()).find(
    m => m.template === "pb_newtab"
  ).content;
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    value: {
      ...defaultMessageContent,
      enabled: true,
      promoLinkType: "button",
      promoSectionStyle: "top",
      promoHeader: "Need more privacy?",
      promoTitle:
        "Mozilla VPN. Security, reliability and speed — on every device, anywhere you go.",
      promoImageLarge: "chrome://browser/content/assets/moz-vpn.svg",
      promoImageSmall: "chrome://browser/content/assets/vpn-logo.svg",
      infoTitleEnabled: false,
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    ok(
      !content.document.querySelector("#info-title"),
      "Should remove the infoTitle element"
    );
    is(
      content.document.querySelector(".promo-image-small img").src,
      "chrome://browser/content/assets/vpn-logo.svg",
      "Should have logo image"
    );
    is(
      content.document.querySelector(".promo-image-large img").src,
      "chrome://browser/content/assets/moz-vpn.svg",
      "Should have a product image"
    );
    ok(
      content.document.querySelector(".promo.top"),
      "Should have .below-search for the promo section"
    );
    ok(
      !content.document.querySelector("#info-title"),
      "Should not render info title if infoTitleEnabled is false"
    );
    ok(
      content.document.querySelector("#private-browsing-vpn-text"),
      "Should render promo title if promoTitleEnabled is true"
    );
  });

  await BrowserTestUtils.closeWindow(win);

  await doExperimentCleanup();
});

add_task(async function test_experiment_messaging_system() {
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pbNewtab",
    enabled: true,
    value: {
      id: "PB_NEWTAB_MESSAGING_SYSTEM",
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
    },
  });
  Services.prefs.setStringPref(
    "browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments",
    '{"id":"messaging-experiments","enabled":true,"type":"remote-experiments","messageGroups":["pbNewtab"],"updateCycleInMs":0}'
  );
  const { ASRouter } = ChromeUtils.import(
    "resource://activity-stream/lib/ASRouter.jsm"
  );
  // Reload the provider
  await ASRouter._updateMessageProviders();
  // Wait to load the messages from the messaging-experiments provider
  await ASRouter.loadMessagesFromAllProviders();

  Assert.ok(
    ASRouter.state.messages.find(m => m.id === "PB_NEWTAB_MESSAGING_SYSTEM"),
    "Experiment message found in ASRouter state"
  );

  Services.telemetry.clearEvents();

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [LOCALE], async function(locale) {
    const infoBody = content.document.getElementById("info-body");
    const promoLink = content.document.getElementById(
      "private-browsing-vpn-link"
    );

    // Check experiment values are rendered
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
      content.document.querySelector(".promo a").getAttribute("href"),
      "http://bar.example.com/" + locale,
      "should format the promoLinkUrl url"
    );
  });

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "expose",
        extra: {
          featureId: "pbNewtab",
        },
      },
    ],
    { category: "normandy" }
  );

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});
