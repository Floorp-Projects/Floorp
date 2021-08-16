/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
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
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      infoTitle: "Hello world",
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
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    enabled: true,
    value: {
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-need-more-privacy",
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
      "Need more privacy?",
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
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    value: {
      enabled: true,
      promoLinkType: "button",
      promoSectionStyle: "bottom",
      promoHeader: "Need more privacy?",
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
    ok(
      content.document.querySelector(".promo.bottom"),
      "Should have .bottom for the promo section"
    );
    ok(
      content.document.querySelector("#info-title"),
      "Should render info title if infoTitleEnabled is true"
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
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    value: {
      enabled: true,
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoHeader: "Need more privacy?",
      promoTitle:
        "Mozilla VPN. Security, reliability and speed — on every device,  anywhere you go.",
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
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatebrowsing",
    value: {
      enabled: true,
      promoLinkType: "button",
      promoSectionStyle: "top",
      promoHeader: "Need more privacy?",
      promoTitle:
        "Mozilla VPN. Security, reliability and speed — on every device, anywhere you go.",
      infoTitleEnabled: false,
    },
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    ok(
      !content.document.querySelector("#info-title"),
      "Should remove the infoTitle element"
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
