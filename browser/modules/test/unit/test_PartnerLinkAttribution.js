/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { PartnerLinkAttribution, CONTEXTUAL_SERVICES_PING_TYPES } =
  ChromeUtils.importESModule(
    "resource:///modules/PartnerLinkAttribution.sys.mjs"
  );

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const FAKE_PING = { tile_id: 1, position: 1 };

let sandbox;
let stub;

add_task(function setup() {
  sandbox = sinon.createSandbox();
  stub = sandbox.stub(
    PartnerLinkAttribution._pingCentre,
    "sendStructuredIngestionPing"
  );
  stub.returns(200);

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(function test_sendContextualService_success() {
  for (const type of Object.values(CONTEXTUAL_SERVICES_PING_TYPES)) {
    PartnerLinkAttribution.sendContextualServicesPing(FAKE_PING, type);

    Assert.ok(stub.calledOnce, `Should send the ping for ${type}`);

    const [payload, endpoint] = stub.firstCall.args;
    Assert.ok(!!payload.context_id, "Should add context_id to the payload");
    Assert.ok(
      endpoint.includes(type),
      "Should include the ping type in the endpoint URL"
    );
    stub.resetHistory();
  }
});

add_task(function test_rejectUnknownPingType() {
  PartnerLinkAttribution.sendContextualServicesPing(FAKE_PING, "unknown-type");

  Assert.ok(stub.notCalled, "Should not send the ping with unknown ping type");
});
