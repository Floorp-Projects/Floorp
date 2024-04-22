/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const sandbox = sinon.createSandbox();

add_setup(async function () {
  ASRouter.resetMessageState();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.promo.pin.enabled", true]],
  });
  await ASRouter.onPrefChange();
  // Stub out the doesAppNeedPin to true so that Pin Promo targeting evaluates true

  sandbox.stub(ShellService, "doesAppNeedPin").withArgs(true).returns(true);
  registerCleanupFunction(async () => {
    sandbox.restore();
  });
});

add_task(async function test_pin_promo() {
  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function () {
    const promoContainer = content.document.querySelector(".promo");
    const promoHeader = content.document.getElementById("promo-header");

    ok(promoContainer, "Pin promo is shown");
    is(
      promoHeader.getAttribute("data-l10n-id"),
      "about-private-browsing-pin-promo-header",
      "Correct default values are shown"
    );
  });

  let { win: win2 } = await openTabAndWaitForRender();
  let { win: win3 } = await openTabAndWaitForRender();
  let { win: win4, tab: tab4 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab4, [], async function () {
    is(
      content.document.getElementById(".private-browsing-promo-link"),
      null,
      "should no longer render the promo after 3 impressions"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win3);
  await BrowserTestUtils.closeWindow(win4);
});

add_task(async function test_pin_promo_mr2022_holdback() {
  ASRouter.resetMessageState();
  // Set majorRelease2022 feature onboarding variable fallback pref
  // for inMr2022Holdback targeting to evaluate true
  await SpecialPowers.pushPrefEnv({
    set: [["browser.majorrelease.onboarding", false]],
  });
  await ASRouter.onPrefChange();
  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function () {
    const promoContainer = content.document.querySelector(".promo");
    const promoButton = content.document.querySelector(
      "#private-browsing-promo-link"
    );

    ok(promoContainer, "Promo is shown");

    Assert.equal(
      promoButton.getAttribute("data-l10n-id"),
      "about-private-browsing-focus-promo-cta",
      "Pin Promo not shown for holdback user"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
});

add_task(async function test_pin_promo_mr2022_not_holdback() {
  ASRouter.resetMessageState();
  // Set majorRelease2022 feature onboarding variable fallback pref
  // for inMr2022Holdback targeting to evaluate false
  await SpecialPowers.pushPrefEnv({
    set: [["browser.majorrelease.onboarding", true]],
  });
  await ASRouter.onPrefChange();
  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function () {
    const promoContainer = content.document.querySelector(".promo");
    const promoHeader = content.document.getElementById("promo-header");

    ok(promoContainer, "Promo is shown");

    is(
      promoHeader.getAttribute("data-l10n-id"),
      "about-private-browsing-pin-promo-header",
      "Pin Promo is shown"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
});
