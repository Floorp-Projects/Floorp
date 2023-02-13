/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);
const { ASRouterTargeting } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);
const { MacAttribution } = ChromeUtils.import(
  "resource:///modules/MacAttribution.jsm"
);
const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

add_task(async function check_attribution_data() {
  // Some setup to fake the correct attribution data
  const appPath = MacAttribution.applicationPath;
  const attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
    Ci.nsIMacAttributionService
  );
  const campaign = "non-fx-button";
  const source = "addons.mozilla.org";
  const referrer = `https://allizom.org/anything/?utm_campaign=${campaign}&utm_source=${source}`;
  attributionSvc.setReferrerUrl(appPath, referrer, true);
  AttributionCode._clearCache();
  await AttributionCode.getAttrDataAsync();

  const {
    campaign: attributionCampain,
    source: attributionSource,
  } = ASRouterTargeting.Environment.attributionData;
  equal(
    attributionCampain,
    campaign,
    "should get the correct campaign out of attributionData"
  );
  equal(
    attributionSource,
    source,
    "should get the correct source out of attributionData"
  );

  const messages = [
    {
      id: "foo1",
      targeting:
        "attributionData.campaign == 'back_to_school' && attributionData.source == 'addons.mozilla.org'",
    },
    {
      id: "foo2",
      targeting:
        "attributionData.campaign == 'non-fx-button' && attributionData.source == 'addons.mozilla.org'",
    },
  ];

  equal(
    await ASRouterTargeting.findMatchingMessage({ messages }),
    messages[1],
    "should select the message with the correct campaign and source"
  );
  AttributionCode._clearCache();
});

add_task(async function check_enterprise_targeting() {
  const messages = [
    {
      id: "foo1",
      targeting: "hasActiveEnterprisePolicies",
    },
    {
      id: "foo2",
      targeting: "!hasActiveEnterprisePolicies",
    },
  ];

  equal(
    await ASRouterTargeting.findMatchingMessage({ messages }),
    messages[1],
    "should select the message for policies turned off"
  );

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      DisableFirefoxStudies: {
        Value: true,
      },
    },
  });

  equal(
    await ASRouterTargeting.findMatchingMessage({ messages }),
    messages[0],
    "should select the message for policies turned on"
  );
});
