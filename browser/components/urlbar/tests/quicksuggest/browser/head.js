/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let sandbox;

/* import-globals-from ../../browser/head-common.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-common.js",
  this
);

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

XPCOMUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: module } = ChromeUtils.import(
    "resource://testing-common/QuickSuggestTestUtils.jsm"
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
