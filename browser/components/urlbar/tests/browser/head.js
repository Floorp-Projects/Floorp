/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the result/url loading functionality of UrlbarController.
 */

"use strict";

let sandbox;

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  QueryContext: "resource:///modules/UrlbarUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarMatch: "resource:///modules/UrlbarMatch.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

registerCleanupFunction(function() {
  delete window.sinon;
});
