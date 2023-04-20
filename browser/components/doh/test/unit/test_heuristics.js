/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

let cid;

async function SetMockParentalControlEnabled(aEnabled) {
  if (cid) {
    MockRegistrar.unregister(cid);
  }

  let parentalControlsService = {
    parentalControlsEnabled: aEnabled,
    QueryInterface: ChromeUtils.generateQI(["nsIParentalControlsService"]),
  };
  cid = MockRegistrar.register(
    "@mozilla.org/parental-controls-service;1",
    parentalControlsService
  );
}

registerCleanupFunction(() => {
  if (cid) {
    MockRegistrar.unregister(cid);
  }
});

add_task(setup);

add_task(async function test_parentalControls() {
  let DoHHeuristics = ChromeUtils.importESModule(
    "resource:///modules/DoHHeuristics.sys.mjs"
  );

  let parentalControls = DoHHeuristics.parentalControls;

  Assert.equal(
    await parentalControls(),
    "enable_doh",
    "By default, parental controls should be disabled and doh should be enabled"
  );

  SetMockParentalControlEnabled(false);

  Assert.equal(
    await parentalControls(),
    "enable_doh",
    "Mocked parental controls service is disabled; doh is enabled"
  );

  SetMockParentalControlEnabled(true);

  Assert.equal(
    await parentalControls(),
    "enable_doh",
    "Default value of mocked parental controls service is disabled; doh is enabled"
  );

  SetMockParentalControlEnabled(false);

  Assert.equal(
    await parentalControls(),
    "enable_doh",
    "Mocked parental controls service is disabled; doh is enabled"
  );

  MockRegistrar.unregister(cid);

  Assert.equal(
    await parentalControls(),
    "enable_doh",
    "By default, parental controls should be disabled and doh should be enabled"
  );
});

add_task(async function test_canary() {
  let DoHHeuristics = ChromeUtils.importESModule(
    "resource:///modules/DoHHeuristics.sys.mjs"
  );
  const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
    Ci.nsINativeDNSResolverOverride
  );

  function setCanary(ip, pre, post) {
    override.clearOverrides();
    override.addIPOverride("use-application-dns.net.", ip);

    if (!pre) {
      pre = "1.2.3.4";
    }
    if (!post) {
      post = "1.2.3.4";
    }

    override.addIPOverride("example.com.", pre);
    override.addIPOverride("example.org.", post);
  }

  setCanary("1.2.3.4");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "enable_doh",
    "Canary should not be triggered"
  );

  setCanary("0.0.0.0");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "disable_doh",
    "Canary should be triggered"
  );
  setCanary("127.0.0.1");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "disable_doh",
    "Canary should be triggered"
  );
  setCanary("::1");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "disable_doh",
    "Canary should be triggered"
  );
  setCanary("192.168.1.1");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "disable_doh",
    "Canary should be triggered"
  );
  setCanary("10.1.1.1");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "disable_doh",
    "Canary should be triggered"
  );
  setCanary("N/A");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "disable_doh",
    "Canary should be triggered"
  );

  Services.io.offline = true;
  setCanary("N/A");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "enable_doh",
    "Canary should not be triggered when offline"
  );

  Services.io.offline = false;
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "disable_doh",
    "Canary should be triggered when online"
  );

  setCanary("N/A", "N/A", "1.2.3.4");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "enable_doh",
    "Canary should not be triggered when precondition fails"
  );

  setCanary("N/A", "1.2.3.4", "N/A");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "enable_doh",
    "Canary should not be triggered when postcondition fails"
  );

  setCanary("N/A", "N/A", "N/A");
  Assert.equal(
    await DoHHeuristics.globalCanary(),
    "enable_doh",
    "Canary should not be triggered when pre&postcondition fails"
  );

  override.clearOverrides();
});
