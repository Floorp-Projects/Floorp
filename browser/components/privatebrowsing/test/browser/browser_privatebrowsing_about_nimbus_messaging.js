/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

/* Tests that use TelemetryTestUtils.assertEvents (at the very least, those with
 * `{ process: "content" }`) seem to be super flaky and intermittent-prone when they
 * share a file with other telemetry tests, so each one gets its own file.
 */

add_task(async function test_experiment_messaging_system() {
  const LOCALE = Services.locale.appLocaleAsBCP47;
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_MESSAGING_SYSTEM",
    template: "pb_newtab",
    content: {
      hideDefault: true,
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

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});
