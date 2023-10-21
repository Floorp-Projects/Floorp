/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

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

  await SpecialPowers.spawn(tab, [LOCALE], async function (locale) {
    const infoBody = content.document.getElementById("info-body");
    const promoLink = content.document.getElementById(
      "private-browsing-promo-link"
    );

    // Check experiment values are rendered
    is(
      infoBody.getAttribute("data-l10n-id"),
      "about-private-browsing-info-title",
      "should render infoBody with fluent"
    );
    is(
      promoLink.getAttribute("data-l10n-id"),
      "about-private-browsing-prominent-cta",
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
  });

  await BrowserTestUtils.closeWindow(win);
  await doExperimentCleanup();
});

add_task(async function test_experiment_promo_action() {
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_TEST_URL",
    template: "pb_newtab",
    content: {
      hideDefault: true,
      promoEnabled: true,
      infoEnabled: true,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com/%LOCALE%",
      promoLinkType: "button",
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
  const sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    ASRouter.resetMessageState();
    sandbox.restore();
    BrowserTestUtils.closeWindow(win);
  });

  let windowGlobalParent =
    win.gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;
  let aboutPrivateBrowsingActor = windowGlobalParent.getActor(
    "AboutPrivateBrowsing"
  );

  let specialActionSpy = sandbox.spy(
    aboutPrivateBrowsingActor,
    "receiveMessage"
  );

  let expectedUrl = "https://foo.example.com";

  await SpecialPowers.spawn(tab, [], async function () {
    ok(
      content.document.querySelector(".promo"),
      "should render the promo experiment message"
    );

    is(
      content.document
        .querySelector(".promo button")
        .classList.contains("primary"),
      true,
      "should render the promo button styled as a button"
    );

    content.document.querySelector(".promo button").click();
    info("promo button clicked");
  });

  Assert.equal(
    specialActionSpy.callCount,
    1,
    "Should be called by promo action"
  );

  let promoAction = specialActionSpy.firstCall.args[0].data;

  Assert.equal(
    promoAction.type,
    "OPEN_URL",
    "Should be called with promo button action"
  );

  Assert.equal(
    promoAction.data.args,
    expectedUrl,
    "Should be called with right URL"
  );

  await doExperimentCleanup();
});

add_task(async function test_experiment_open_spotlight_action() {
  let doExperimentCleanup = await setupMSExperimentWithMessage({
    id: "PB_NEWTAB_TEST_SPOTLIGHT",
    template: "pb_newtab",
    content: {
      hideDefault: true,
      promoEnabled: true,
      infoEnabled: true,
      infoBody: "fluent:about-private-browsing-info-title",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      infoLinkUrl: "http://foo.example.com/",
      promoLinkType: "button",
      promoButton: {
        action: {
          type: "SHOW_SPOTLIGHT",
          data: {
            content: {
              template: "multistage",
              screens: [
                {
                  content: {
                    title: "Test",
                    subtitle: "Sub Title",
                  },
                },
              ],
            },
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
  const sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    ASRouter.resetMessageState();
    sandbox.restore();
    BrowserTestUtils.closeWindow(win);
  });

  let windowGlobalParent =
    win.gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;
  let aboutPrivateBrowsingActor = windowGlobalParent.getActor(
    "AboutPrivateBrowsing"
  );

  let specialActionSpy = sandbox.spy(
    aboutPrivateBrowsingActor,
    "receiveMessage"
  );

  await SpecialPowers.spawn(tab, [], async function () {
    ok(
      content.document.querySelector(".promo"),
      "should render the promo experiment message"
    );
    content.document.querySelector(".promo button").click();
  });

  Assert.equal(
    specialActionSpy.callCount,
    1,
    "Should be called by promo action"
  );

  let promoAction = specialActionSpy.firstCall.args[0].data;

  Assert.equal(
    promoAction.type,
    "SHOW_SPOTLIGHT",
    "Should be called with promo button spotlight action"
  );

  Assert.equal(
    promoAction.data.content.metrics,
    "allow",
    "Should be called with metrics property set as allow for experiments"
  );

  await doExperimentCleanup();
});
