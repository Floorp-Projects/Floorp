/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test we don't re-enter record() (and record both an engagement and an
// abandonment) when handling an engagement blurs the input field.

const TEST_URL = "https://example.com/";

add_task(async function () {
  await setup();
  let legacyEngagementDeferred = Promise.withResolvers();
  let onEngagementDeferred = Promise.withResolvers();
  const provider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: TEST_URL,
          helpUrl: "https://example.com/help",
          helpL10n: {
            id: "urlbar-result-menu-tip-get-help",
          },
        }
      ),
    ],
    priority: 999,
    onLegacyEngagement: () => {
      info("Blur the address bar during the onLegacyEngagement notification");
      gURLBar.blur();
      // Run at the next tick to be sure spurious events would have happened.
      TestUtils.waitForTick().then(() => {
        legacyEngagementDeferred.resolve();
      });
    },
    onEngagement: () => {
      info("Blur the address bar during the onEngagement notification");
      gURLBar.blur();
      // Run at the next tick to be sure spurious events would have happened.
      TestUtils.waitForTick().then(() => {
        onEngagementDeferred.resolve();
      });
    },
  });
  UrlbarProvidersManager.registerProvider(provider);
  // This should cover at least engagement and abandonment.
  let legacyEngagementSpy = sinon.spy(provider, "onLegacyEngagement");
  let engagementSpy = sinon.spy(provider, "onEngagement");

  let beforeRecordCall = false,
    recordReentered = false;
  let recordStub = sinon
    .stub(gURLBar.controller.engagementEvent, "record")
    .callsFake((...args) => {
      recordReentered = beforeRecordCall;
      beforeRecordCall = true;
      recordStub.wrappedMethod.apply(gURLBar.controller.engagementEvent, args);
      beforeRecordCall = false;
    });

  registerCleanupFunction(() => {
    sinon.restore();
    UrlbarProvidersManager.unregisterProvider(provider);
  });

  await doTest(async () => {
    await openPopup("example");
    await selectRowByURL(TEST_URL);
    EventUtils.synthesizeKey("VK_RETURN");
    await Promise.all([legacyEngagementDeferred, onEngagementDeferred]);

    assertEngagementTelemetry([{ engagement_type: "enter" }]);
    assertAbandonmentTelemetry([]);

    Assert.ok(recordReentered, "`record()` was re-entered");
    Assert.equal(
      legacyEngagementSpy.callCount,
      1,
      "`onLegacyEngagement` was invoked once"
    );
    Assert.equal(
      legacyEngagementSpy.args[0][0],
      "engagement",
      "`engagement` notified"
    );
    Assert.equal(engagementSpy.callCount, 1, "`onEngagement` was invoked once");
  });
});
