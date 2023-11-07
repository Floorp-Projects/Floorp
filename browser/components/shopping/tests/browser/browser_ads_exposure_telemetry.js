/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

const ADS_JSON = `[{
	"name": "Test product name ftw",
	"url": ${PRODUCT_PAGE},
	"image_url": "https://i.fakespot.io/b6vx27xf3rgwr1a597q6qd3rutp6",
	"price": "249.99",
	"currency": "USD",
	"grade": "A",
	"adjusted_rating": 4.6,
	"analysis_url": "https://www.fakespot.com/product/test-product",
	"sponsored": true,
  "aid": "a2VlcCBvbiByb2NraW4gdGhlIGZyZWUgd2ViIQ==",
}]`;

// Verifies that, if the ads server returns an ad, but we have disabled
// ads exposure, no Glean telemetry is recorded.
add_task(async function test_ads_exposure_disabled_not_recorded() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.ads.enabled", false],
      ["browser.shopping.experience2023.ads.exposure", false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [PRODUCT_PAGE, ADS_JSON],
        async (prodPage, adResponse) => {
          const { ShoppingProduct } = ChromeUtils.importESModule(
            "chrome://global/content/shopping/ShoppingProduct.mjs"
          );
          const { sinon } = ChromeUtils.importESModule(
            "resource://testing-common/Sinon.sys.mjs"
          );

          let productURI = Services.io.newURI(prodPage);
          let product = new ShoppingProduct(productURI);
          let productRequestAdsStub = sinon.stub(
            product,
            "requestRecommendations"
          );
          productRequestAdsStub.resolves(adResponse);

          let actor = content.windowGlobalChild.getActor("ShoppingSidebar");
          actor.productURI = productURI;
          actor.product = product;

          actor.requestRecommendations(productURI);

          Assert.ok(
            productRequestAdsStub.notCalled,
            "product.requestRecommendations should not have been called if ads and ads exposure were disabled"
          );
        }
      );
    }
  );

  await Services.fog.testFlushAllChildren();

  var events = Glean.shopping.adsExposure.testGetValue();
  Assert.equal(events, null, "Ads exposure should not have been recorded");
  await SpecialPowers.popPrefEnv();
});

// Verifies that, if the ads server returns nothing, and ads exposure is
// enabled, no Glean telemetry is recorded.
add_task(async function test_ads_exposure_enabled_no_ad_not_recorded() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.ads.enabled", true],
      ["browser.shopping.experience2023.ads.exposure", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [PRODUCT_PAGE], async prodPage => {
        const { ShoppingProduct } = ChromeUtils.importESModule(
          "chrome://global/content/shopping/ShoppingProduct.mjs"
        );
        const { sinon } = ChromeUtils.importESModule(
          "resource://testing-common/Sinon.sys.mjs"
        );

        let productURI = Services.io.newURI(prodPage);
        let product = new ShoppingProduct(productURI);
        let productRequestAdsStub = sinon.stub(
          product,
          "requestRecommendations"
        );
        productRequestAdsStub.resolves([]);

        let actor = content.windowGlobalChild.getActor("ShoppingSidebar");
        actor.productURI = productURI;
        actor.product = product;

        actor.requestRecommendations(productURI);

        Assert.ok(
          productRequestAdsStub.called,
          "product.requestRecommendations should have been called"
        );
      });
    }
  );

  await Services.fog.testFlushAllChildren();

  var events = Glean.shopping.adsExposure.testGetValue();
  Assert.equal(
    events,
    null,
    "Ads exposure should not have been recorded if ads exposure was enabled but no ads were returned"
  );
  await SpecialPowers.popPrefEnv();
});

// Verifies that, if ads are disabled but ads exposure is enabled, ads will
// be fetched, and if an ad is returned, the Glean probe will be recorded.
add_task(async function test_ads_exposure_enabled_with_ad_recorded() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.ads.enabled", false],
      ["browser.shopping.experience2023.ads.exposure", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [PRODUCT_PAGE, ADS_JSON],
        async (prodPage, adResponse) => {
          const { ShoppingProduct } = ChromeUtils.importESModule(
            "chrome://global/content/shopping/ShoppingProduct.mjs"
          );
          const { sinon } = ChromeUtils.importESModule(
            "resource://testing-common/Sinon.sys.mjs"
          );

          let productURI = Services.io.newURI(prodPage);
          let product = new ShoppingProduct(productURI);
          let productRequestAdsStub = sinon.stub(
            product,
            "requestRecommendations"
          );
          productRequestAdsStub.resolves(adResponse);

          let actor = content.windowGlobalChild.getActor("ShoppingSidebar");
          actor.productURI = productURI;
          actor.product = product;

          actor.requestRecommendations(productURI);

          Assert.ok(
            productRequestAdsStub.called,
            "product.requestRecommendations should have been called if ads exposure is enabled, even if ads are not"
          );
        }
      );
    }
  );

  await Services.fog.testFlushAllChildren();

  const events = Glean.shopping.adsExposure.testGetValue();
  Assert.equal(
    events.length,
    1,
    "Ads exposure should have been recorded if ads exposure was enabled and ads were returned"
  );
  Assert.equal(
    events[0].category,
    "shopping",
    "Glean event should have category 'shopping'"
  );
  Assert.equal(
    events[0].name,
    "ads_exposure",
    "Glean event should have name 'ads_exposure'"
  );
  await SpecialPowers.popPrefEnv();
});
