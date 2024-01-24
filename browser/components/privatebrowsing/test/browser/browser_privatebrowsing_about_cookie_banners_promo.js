const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

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
      // The message's targeting is looking for the following prefs not being 0
      ["cookiebanners.service.mode", 0],
      ["cookiebanners.service.mode.privateBrowsing", 0],
    ],
  });
  await ASRouter.onPrefChange();

  const { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [promoImgSrc], async function (imgSrc) {
    const promoImage = content.document.querySelector(
      ".promo-image-large > img"
    );
    Assert.notStrictEqual(
      promoImage?.src,
      imgSrc,
      "Cookie banner reduction promo is not shown"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_cookie_banners_promo() {
  await resetState();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.promo.cookiebanners.enabled", true],
      ["cookiebanners.service.mode.privateBrowsing", 1],
    ],
  });
  await ASRouter.onPrefChange();

  const sandbox = sinon.createSandbox();
  const expectedUrl = Services.urlFormatter.formatURL(
    "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cookie-banner-reduction"
  );

  const { win, tab } = await openTabAndWaitForRender();
  let triedToOpenTab = new Promise(resolve => {
    sandbox.stub(win, "openLinkIn").callsFake((url, where) => {
      is(url, expectedUrl, "The link should open the expected URL");
      is(
        where,
        "tabshifted",
        "The link should open the expected URL in a new foreground tab"
      );
      resolve();
    });
  });

  await SpecialPowers.spawn(tab, [promoImgSrc], async function (imgSrc) {
    const promoImage = content.document.querySelector(
      ".promo-image-large > img"
    );
    Assert.strictEqual(
      promoImage?.src,
      imgSrc,
      "Cookie banner reduction promo is shown"
    );
    let linkEl = content.document.getElementById("private-browsing-promo-link");
    linkEl.click();
  });

  await triedToOpenTab;
  sandbox.restore();

  ok(true, "The link was clicked and the new tab opened");

  let { win: win2, tab: tab2 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab2, [promoImgSrc], async function (imgSrc) {
    const promoImage = content.document.querySelector(
      ".promo-image-large > img"
    );
    Assert.notStrictEqual(
      promoImage?.src,
      imgSrc,
      "Cookie banner reduction promo is no longer shown after clicking the link"
    );
  });

  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});
