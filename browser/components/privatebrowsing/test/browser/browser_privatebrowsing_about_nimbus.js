/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

add_task(async function test_experiment_plain_text() {
  const defaultMessageContent = (await PanelTestProvider.getMessages()).find(
    m => m.template === "pb_newtab"
  ).content;
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      ...defaultMessageContent,
      infoTitle: "Hello world",
      infoTitleEnabled: true,
      infoBody: "This is some text",
      infoLinkText: "This is a link",
      infoIcon: "chrome://branding/content/about-logo.png",
      promoTitle: "Promo title",
      promoLinkText: "Promo link",
      promoLinkType: "link",
      promoButton: {
        action: {
          type: "OPEN_URL",
          data: {
            args: "https://example.com",
            where: "tabshifted",
          },
        },
      },
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    const infoContainer = content.document.querySelector(".info");
    const infoTitle = content.document.getElementById("info-title");
    const infoBody = content.document.getElementById("info-body");
    const infoLink = content.document.getElementById("private-browsing-myths");
    const promoText = content.document.getElementById(
      "private-browsing-promo-text"
    );
    const promoLink = content.document.getElementById(
      "private-browsing-promo-link"
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

add_task(async function test_experiment_info_disabled() {
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      infoEnabled: false,
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
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
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      promoEnabled: false,
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
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
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      infoEnabled: true,
      promoEnabled: true,
      infoLinkUrl: "http://foo.mozilla.com/%LOCALE%",
      promoButton: {
        action: {
          data: {
            args: "http://bar.mozilla.com/%LOCALE%",
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

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [LOCALE], async function(locale) {
    is(
      content.document.querySelector(".info a").getAttribute("href"),
      "http://foo.mozilla.com/" + locale,
      "should format the infoLinkUrl url"
    );

    ok(
      content.document.querySelector(".promo button"),
      "should render promo button"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_click_info_telemetry() {
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM_CLICK_INFO_TELEM",
    template: "pb_newtab",
    content: {
      infoEnabled: true,
      infoLinkUrl: "http://example.com",
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  // Required for `mach test --verify`
  Services.telemetry.clearEvents();

  let { win, tab } = await openTabAndWaitForRender();

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
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: `PB_NEWTAB_MESSAGING_SYSTEM_PROMO_TELEM_${Math.random()}`,
    template: "pb_newtab",
    content: {
      promoEnabled: true,
      promoLinkType: "link",
      promoButton: {
        action: {
          type: "OPEN_URL",
          data: {
            args: "https://example.com",
            where: "tabshifted",
          },
        },
      },
    },
    // Priority ensures this message is picked over the one in
    // OnboardingMessageProvider
    priority: 5,
    targeting: "true",
  });

  let { win, tab } = await openTabAndWaitForRender();

  Services.telemetry.clearEvents();

  await SpecialPowers.spawn(tab, [], () => {
    is(
      content.document
        .querySelector(".promo-cta button")
        .classList.contains("promo-link"),
      true,
      "Should have a button styled as a link"
    );

    const el = content.document.querySelector(".promo button");
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

  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      ...defaultMessageContent,
      promoEnabled: true,
      promoLinkType: "button",
      promoSectionStyle: "bottom",
      promoHeader: "Need more privacy?",
      infoTitleEnabled: true,
      promoTitleEnabled: false,
      promoImageLarge: "",
      promoImageSmall: "chrome://browser/content/assets/vpn-logo.svg",
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

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    is(
      content.document
        .querySelector(".promo-cta button")
        .classList.contains("primary"),
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
      content.document.querySelector("#info-title"),
      "Should render info title if infoTitleEnabled is true"
    );
    ok(
      !content.document.querySelector("#private-browsing-promo-text"),
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
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      ...defaultMessageContent,
      promoEnabled: true,
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoHeader: "Need more privacy?",
      promoTitle:
        "Mozilla VPN. Security, reliability and speed — on every device, anywhere you go.",
      promoImageLarge: "chrome://browser/content/assets/moz-vpn.svg",
      promoImageSmall: "chrome://browser/content/assets/vpn-logo.svg",
      infoTitleEnabled: false,
      promoButton: {
        action: {
          data: {
            args: "https://foo.example.com",
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

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    is(
      content.document
        .querySelector(".promo-cta button")
        .classList.contains("primary"),
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
      content.document.querySelector("#private-browsing-promo-text"),
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
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: `PB_NEWTAB_MESSAGING_SYSTEM_DISMISS_${Math.random()}`,
    template: "pb_newtab",
    content: {
      ...defaultMessageContent,
      promoEnabled: true,
      promoLinkType: "button",
      promoSectionStyle: "top",
      promoHeader: "Need more privacy?",
      promoTitle:
        "Mozilla VPN. Security, reliability and speed — on every device, anywhere you go.",
      promoImageLarge: "chrome://browser/content/assets/moz-vpn.svg",
      promoImageSmall: "chrome://browser/content/assets/vpn-logo.svg",
      infoTitleEnabled: false,
      promoButton: {
        action: {
          data: {
            args: "https://foo.example.com",
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
      content.document.querySelector("#private-browsing-promo-text"),
      "Should render promo title if promoTitleEnabled is true"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});
