/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AboutWelcomeDefaults } = ChromeUtils.import(
  "resource://activity-stream/aboutwelcome/lib/AboutWelcomeDefaults.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);
const { AddonRepository } = ChromeUtils.import(
  "resource://gre/modules/addons/AddonRepository.jsm"
);

const TEST_ATTRIBUTION_DATA = {
  source: "addons.mozilla.org",
  medium: "referral",
  campaign: "non-fx-button",
  content: "iridium%40particlecore.github.io",
};

add_task(async function test_handleAddonInfoNotFound() {
  let sandbox = sinon.createSandbox();
  const stub = sandbox.stub(AttributionCode, "getAttrDataAsync").resolves(null);
  let result = await AboutWelcomeDefaults.getAttributionContent();
  equal(stub.callCount, 1, "Call was made");
  equal(result, null, "No data is returned");

  sandbox.restore();
});

add_task(async function test_UAAttribution() {
  let sandbox = sinon.createSandbox();
  const stub = sandbox
    .stub(AttributionCode, "getAttrDataAsync")
    .resolves({ ua: "test" });
  let result = await AboutWelcomeDefaults.getAttributionContent();
  equal(stub.callCount, 1, "Call was made");
  equal(result.template, undefined, "Template was not returned");
  equal(result.ua, "test", "UA was returned");

  sandbox.restore();
});

add_task(async function test_formatAttributionData() {
  let sandbox = sinon.createSandbox();
  const TEST_ADDON_INFO = {
    sourceURI: { scheme: "https", spec: "https://test.xpi" },
    name: "Test Add-on",
    icons: { "64": "http://test.svg" },
  };
  sandbox
    .stub(AttributionCode, "getAttrDataAsync")
    .resolves(TEST_ATTRIBUTION_DATA);
  sandbox.stub(AddonRepository, "getAddonsByIDs").resolves([TEST_ADDON_INFO]);
  let result = await AboutWelcomeDefaults.getAttributionContent(
    TEST_ATTRIBUTION_DATA
  );
  equal(AddonRepository.getAddonsByIDs.callCount, 1, "Retrieve addon content");
  equal(result.template, "return_to_amo", "RTAMO template returned");
  equal(result.name, TEST_ADDON_INFO.name, "AddonInfo returned");

  sandbox.restore();
});
