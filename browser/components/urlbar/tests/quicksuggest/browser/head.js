/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let sandbox;

/* import-globals-from ../../browser/head-common.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-common.js",
  this
);

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/QuickSuggestTestUtils.sys.mjs"
  );
  module.init(this);
  registerCleanupFunction(() => module.uninit());
  return module;
});

registerCleanupFunction(async () => {
  // Ensure the popup is always closed at the end of each test to avoid
  // interfering with the next test.
  await UrlbarTestUtils.promisePopupClose(window);
});

const ONBOARDING_URI =
  "chrome://browser/content/urlbar/quicksuggestOnboarding.html";
