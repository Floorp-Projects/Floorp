/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the result/url loading functionality of UrlbarController.
 */

"use strict";

let sandbox;

var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/* import-globals-from head-common.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-common.js",
  this);

const {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");

registerCleanupFunction(async () => {
  // Ensure the Urlbar popup is always closed at the end of a test, to save having
  // to do it within each test.
  await UrlbarTestUtils.promisePopupClose(window);
});
