/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is an integration test from navigator.mozLoop through to the end
 * effects - rather than just testing MozLoopAPI alone.
 */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
const {SocialService} = Cu.import("resource://gre/modules/SocialService.jsm", {});

add_task(loadLoopPanel);

const kShareWidgetId = "social-share-button";
const kShareProvider = {
  name: "provider 1",
  origin: "https://example.com",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png",
  shareURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar_empty.html"
};
const kShareProviderInvalid = {
  name: "provider 1",
  origin: "https://example2.com"
};

registerCleanupFunction(function* () {
  yield new Promise(resolve => SocialService.disableProvider(kShareProvider.origin, resolve));
  yield new Promise(resolve => SocialService.disableProvider(kShareProviderInvalid.origin, resolve));
  Assert.strictEqual(Social.providers.length, 0, "all providers should be removed");
  SocialShare.uninit();
});

add_task(function* test_mozLoop_isSocialShareButtonAvailable() {
  Assert.ok(gMozLoopAPI, "mozLoop should exist");

  // First make sure the Social Share button is not available. This is probably
  // already the case, but make it explicit here.
  CustomizableUI.removeWidgetFromArea(kShareWidgetId);

  Assert.ok(!gMozLoopAPI.isSocialShareButtonAvailable(),
    "Social Share button should not be available");

  // Add the widget to the navbar.
  CustomizableUI.addWidgetToArea(kShareWidgetId, CustomizableUI.AREA_NAVBAR);

  Assert.ok(gMozLoopAPI.isSocialShareButtonAvailable(),
    "Social Share button should be available");

  // Add the widget to the MenuPanel.
  CustomizableUI.addWidgetToArea(kShareWidgetId, CustomizableUI.AREA_PANEL);

  Assert.ok(gMozLoopAPI.isSocialShareButtonAvailable(),
    "Social Share button should still be available");

  // Test button removal during the same session.
  CustomizableUI.removeWidgetFromArea(kShareWidgetId);

  Assert.ok(!gMozLoopAPI.isSocialShareButtonAvailable(),
    "Social Share button should not be available");
});

add_task(function* test_mozLoop_addSocialShareButton() {
  gMozLoopAPI.addSocialShareButton();

  Assert.ok(gMozLoopAPI.isSocialShareButtonAvailable(),
    "Social Share button should be available");

  let widget = CustomizableUI.getWidget(kShareWidgetId);
  Assert.strictEqual(widget.areaType, CustomizableUI.TYPE_TOOLBAR,
    "Social Share button should be placed in the navbar");

  CustomizableUI.removeWidgetFromArea(kShareWidgetId);
});

add_task(function* test_mozLoop_addSocialShareProvider() {
  gMozLoopAPI.addSocialShareButton();

  gMozLoopAPI.addSocialShareProvider();

  yield promiseWaitForCondition(() => SocialShare.panel.state == "open");

  Assert.equal(SocialShare.iframe.getAttribute("src"), "about:providerdirectory",
    "Provider directory page should be visible");

  SocialShare.panel.hidePopup();
  CustomizableUI.removeWidgetFromArea(kShareWidgetId);
});

add_task(function* test_mozLoop_getSocialShareProviders() {
  Assert.strictEqual(gMozLoopAPI.getSocialShareProviders().length, 0,
    "Provider list should be empty initially");

  // Add a provider.
  yield new Promise(resolve => SocialService.addProvider(kShareProvider, resolve));

  let providers = gMozLoopAPI.getSocialShareProviders();
  Assert.strictEqual(providers.length, 1,
    "The newly added provider should be part of the list");
  let provider = providers[0];
  Assert.strictEqual(provider.iconURL, kShareProvider.iconURL, "Icon URLs should match");
  Assert.strictEqual(provider.name, kShareProvider.name, "Names should match");
  Assert.strictEqual(provider.origin, kShareProvider.origin, "Origins should match");

  // Add another provider that should not be picked up by Loop.
  yield new Promise(resolve => SocialService.addProvider(kShareProviderInvalid, resolve));

  providers = gMozLoopAPI.getSocialShareProviders();
  Assert.strictEqual(providers.length, 1,
    "The newly added provider should not be part of the list");

  // Let's add a valid second provider object.
  let provider2 = Object.create(kShareProvider);
  provider2.name = "Wildly different name";
  provider2.origin = "https://example3.com";
  yield new Promise(resolve => SocialService.addProvider(provider2, resolve));

  providers = gMozLoopAPI.getSocialShareProviders();
  Assert.strictEqual(providers.length, 2,
    "The newly added provider should be part of the list");
  Assert.strictEqual(providers[1].name, provider2.name,
    "Providers should be ordered alphabetically");

  // Remove the second valid provider.
  yield new Promise(resolve => SocialService.disableProvider(provider2.origin, resolve));
  providers = gMozLoopAPI.getSocialShareProviders();
  Assert.strictEqual(providers.length, 1,
    "The uninstalled provider should not be part of the list");
  Assert.strictEqual(providers[0].name, kShareProvider.name, "Names should match");
});

add_task(function* test_mozLoop_socialShareRoom() {
  gMozLoopAPI.addSocialShareButton();

  gMozLoopAPI.socialShareRoom(kShareProvider.origin, "https://someroom.com", "Some Title");

  yield promiseWaitForCondition(() => SocialShare.panel.state == "open");

  Assert.equal(SocialShare.iframe.getAttribute("origin"), kShareProvider.origin,
    "Origins should match");
  Assert.equal(SocialShare.iframe.getAttribute("src"), kShareProvider.shareURL,
    "Provider's share page should be displayed");

  SocialShare.panel.hidePopup();
  CustomizableUI.removeWidgetFromArea(kShareWidgetId);
});
