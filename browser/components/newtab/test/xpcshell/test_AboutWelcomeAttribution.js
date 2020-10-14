/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AboutWelcomeChild } = ChromeUtils.import(
  "resource:///actors/AboutWelcomeChild.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const TEST_ATTRIBUTION_DATA = {
  source: "addons.mozilla.org",
  medium: "referral",
  campaign: "non-fx-button",
  content: "iridium%40particlecore.github.io",
};

add_task(async function test_handleAddonInfoNotFound() {
  let AWChild = new AboutWelcomeChild();
  const stub = sinon.stub(AWChild, "getAddonInfo").resolves(null);
  let result = await AWChild.formatAttributionData(TEST_ATTRIBUTION_DATA);
  equal(stub.callCount, 1, "Call was made");
  equal(result.template, undefined, "No template returned");
});

add_task(async function test_formatAttributionData() {
  let AWChild = new AboutWelcomeChild();
  const TEST_ADDON_INFO = {
    name: "Test Add-on",
    url: "https://test.xpi",
    iconURL: "http://test.svg",
  };
  sinon.stub(AWChild, "getAddonInfo").resolves(TEST_ADDON_INFO);
  let result = await AWChild.formatAttributionData(TEST_ATTRIBUTION_DATA);
  equal(result.template, "return_to_amo", "RTAMO template returned");
  equal(result.extraProps, TEST_ADDON_INFO, "AddonInfo returned");
});
