const { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const initialHomeRegion = Region._home;
const intialCurrentRegion = Region._current;

// Helper to run tests for specific regions
async function setupRegions(home, current) {
  Region._setHomeRegion(home || "");
  Region._setCurrentRegion(current || "");
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
