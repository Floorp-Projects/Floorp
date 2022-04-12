const { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const initialHomeRegion = Region._home;
const intialCurrentRegion = Region._current;
const initialLocale = Services.locale.appLocaleAsBCP47;

// Helper to run tests for specific regions
async function setupRegions(home, current) {
  Region._setHomeRegion(home || "");
  Region._setCurrentRegion(current || "");
}

// Helper to run tests for specific locales
function setLocale(locale) {
  Services.locale.availableLocales = [locale];
  Services.locale.requestedLocales = [locale];
}

add_task(async function test_focus_promo_in_allowed_region() {
  await ASRouter._resetMessageState();

  const allowedRegion = "ES"; // Spain
  setupRegions(allowedRegion, allowedRegion);

  const { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    const promoContainer = content.document.querySelector(".promo"); // container which is present if promo is enabled and should show

    ok(promoContainer, "Focus promo is shown for allowed region");
  });

  await BrowserTestUtils.closeWindow(win);
  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
});

add_task(async function test_focus_promo_in_disallowed_region() {
  await ASRouter._resetMessageState();

  const disallowedRegion = "CN"; // China
  setupRegions(disallowedRegion);

  const { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    const promoContainer = content.document.querySelector(".promo"); // container which is removed if promo is disabled and/or should not show

    ok(!promoContainer, "Focus promo is not shown for disallowed region");
  });

  await BrowserTestUtils.closeWindow(win);
  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
});

add_task(async function test_focus_promo_in_DE_region_with_English_locale() {
  await ASRouter.resetMessageState();

  const testRegion = "DE"; // Germany
  const testLocale = "en-GB"; // British English
  setupRegions(testRegion);
  setLocale(testLocale);

  const { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function() {
    const buttonText = content.document.querySelector(
      "#private-browsing-vpn-link"
    ).textContent;
    Assert.equal(
      buttonText,
      "Download Firefox Klar",
      "The promo button text should read 'Download Firefox Klar'"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  setLocale(initialLocale); // revert changes to locale
});
