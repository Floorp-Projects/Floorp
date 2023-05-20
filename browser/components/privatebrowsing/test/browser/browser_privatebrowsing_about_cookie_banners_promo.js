const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

let { MODE_REJECT } = Ci.nsICookieBannerService;

const promoImgSrc = "chrome://browser/content/assets/cookie-banners-begone.svg";

async function resetState() {
  await Promise.all([ASRouter.resetMessageState(), ASRouter.unblockAll()]);
}

add_setup(async function setup() {
  registerCleanupFunction(resetState);
  await resetState();
});

add_task(async function test_cookie_banners_promo_user_set_prefs() {
  await resetState();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.promo.cookiebanners.enabled", true],
      // The message's targeting is looking for the following pref being default
      ["cookiebanners.service.mode", MODE_REJECT],
    ],
  });
  await ASRouter.onPrefChange();

  const { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [promoImgSrc], async function (imgSrc) {
    const promoImage = content.document.querySelector(
      ".promo-image-large > img"
    );
    ok(
      promoImage?.src !== imgSrc,
      "Cookie banner reduction promo is not shown"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_cookie_banners_promo() {
  await resetState();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.promo.cookiebanners.enabled", true]],
    clear: [["cookiebanners.service.mode"]],
  });
  await ASRouter.onPrefChange();

  const { win, tab } = await openTabAndWaitForRender();

  let prefChanged = TestUtils.waitForPrefChange(
    "cookiebanners.service.mode",
    value => value === MODE_REJECT
  );
  let pageReloaded = BrowserTestUtils.browserLoaded(tab);

  await SpecialPowers.spawn(tab, [promoImgSrc], async function (imgSrc) {
    const promoImage = content.document.querySelector(
      ".promo-image-large > img"
    );
    ok(promoImage?.src === imgSrc, "Cookie banner reduction promo is shown");
    let linkEl = content.document.getElementById("private-browsing-promo-link");
    EventUtils.synthesizeClick(linkEl);
  });

  await Promise.all([prefChanged, pageReloaded]);

  await SpecialPowers.spawn(tab, [promoImgSrc], async function (imgSrc) {
    const promoImage = content.document.querySelector(
      ".promo-image-large > img"
    );
    ok(
      promoImage?.src !== imgSrc,
      "Cookie banner reduction promo is no longer shown after clicking the link"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});
