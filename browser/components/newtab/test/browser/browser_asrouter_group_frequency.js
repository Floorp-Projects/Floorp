const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
const { CFRMessageProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/CFRMessageProvider.sys.mjs"
);

/**
 * Load and modify a message for the test.
 */
add_setup(async function () {
  const initialMsgCount = ASRouter.state.messages.length;
  const heartbeatMsg = (await CFRMessageProvider.getMessages()).find(
    m => m.id === "HEARTBEAT_TACTIC_2"
  );
  const testMessage = {
    ...heartbeatMsg,
    groups: ["messaging-experiments"],
    targeting: "true",
    // Ensure no overlap due to frequency capping with other tests
    id: `HEARTBEAT_MESSAGE_${Date.now()}`,
  };
  const client = RemoteSettings("cfr");
  await client.db.importChanges({}, Date.now(), [testMessage], {
    clear: true,
  });

  // Force the CFR provider cache to 0 by modifying updateCycleInMs
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.cfr",
        `{"id":"cfr","enabled":true,"type":"remote-settings","collection":"cfr","updateCycleInMs":0}`,
      ],
    ],
  });

  // Reload the providers
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();
  await BrowserTestUtils.waitForCondition(
    () => ASRouter.state.messages.length > initialMsgCount,
    "Should load the extra heartbeat message"
  );

  BrowserTestUtils.waitForCondition(
    () => ASRouter.state.messages.find(m => m.id === testMessage.id),
    "Wait to load the message"
  );

  const msg = ASRouter.state.messages.find(m => m.id === testMessage.id);
  Assert.equal(msg.targeting, "true");
  Assert.equal(msg.groups[0], "messaging-experiments");

  registerCleanupFunction(async () => {
    await client.db.clear();
    // Reload the providers
    await ASRouter._updateMessageProviders();
    await ASRouter.loadMessagesFromAllProviders();
    await BrowserTestUtils.waitForCondition(
      () => ASRouter.state.messages.length === initialMsgCount,
      "Should reset messages"
    );
    await SpecialPowers.popPrefEnv();
  });
});

/**
 * Test group frequency capping.
 * Message has a lifetime frequency of 3 but it's group has a lifetime frequency
 * of 2. It should only show up twice.
 * We update the provider to remove any daily limitations so it should show up
 * on every new tab load.
 */
add_task(async function test_heartbeat_tactic_2() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const TEST_URL = "http://example.com";
  const msg = ASRouter.state.messages.find(m =>
    m.groups.includes("messaging-experiments")
  );
  Assert.ok(msg, "Message found");
  const groupConfiguration = {
    id: "messaging-experiments",
    enabled: true,
    frequency: { lifetime: 2 },
  };
  const client = RemoteSettings("message-groups");
  await client.db.importChanges({}, Date.now(), [groupConfiguration], {
    clear: true,
  });

  // Force the WNPanel provider cache to 0 by modifying updateCycleInMs
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.message-groups",
        `{"id":"message-groups","enabled":true,"type":"remote-settings","collection":"message-groups","updateCycleInMs":0}`,
      ],
    ],
  });

  await BrowserTestUtils.waitForCondition(async () => {
    const msgs = await client.get();
    return msgs.find(m => m.id === groupConfiguration.id);
  }, "Wait for RS message");

  // Reload the providers
  await ASRouter._updateMessageProviders();
  await ASRouter.loadAllMessageGroups();

  let groupState = await BrowserTestUtils.waitForCondition(
    () => ASRouter.state.groups.find(g => g.id === groupConfiguration.id),
    "Wait for group config to load"
  );
  Assert.ok(groupState, "Group config found");
  Assert.ok(groupState.enabled, "Group is enabled");

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  BrowserTestUtils.startLoadingURIString(tab1.linkedBrowser, TEST_URL);

  let chiclet = document.getElementById("contextual-feature-recommendation");
  Assert.ok(chiclet, "CFR chiclet element found (tab1)");
  await BrowserTestUtils.waitForCondition(
    () => !chiclet.hidden,
    "Chiclet should be visible (tab1)"
  );

  await BrowserTestUtils.waitForCondition(
    () =>
      ASRouter.state.messageImpressions[msg.id] &&
      ASRouter.state.messageImpressions[msg.id].length === 1,
    "First impression recorded"
  );

  BrowserTestUtils.removeTab(tab1);

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  BrowserTestUtils.startLoadingURIString(tab2.linkedBrowser, TEST_URL);

  Assert.ok(chiclet, "CFR chiclet element found (tab2)");
  await BrowserTestUtils.waitForCondition(
    () => !chiclet.hidden,
    "Chiclet should be visible (tab2)"
  );

  await BrowserTestUtils.waitForCondition(
    () =>
      ASRouter.state.messageImpressions[msg.id] &&
      ASRouter.state.messageImpressions[msg.id].length === 2,
    "Second impression recorded"
  );

  Assert.ok(
    !ASRouter.isBelowFrequencyCaps(msg),
    "Should have reached freq limit"
  );

  BrowserTestUtils.removeTab(tab2);

  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  BrowserTestUtils.startLoadingURIString(tab3.linkedBrowser, TEST_URL);

  await BrowserTestUtils.waitForCondition(
    () => chiclet.hidden,
    "Heartbeat button should be hidden"
  );
  Assert.equal(
    ASRouter.state.messageImpressions[msg.id] &&
      ASRouter.state.messageImpressions[msg.id].length,
    2,
    "Number of impressions did not increase"
  );

  BrowserTestUtils.removeTab(tab3);

  info("Cleanup");
  await client.db.clear();
  // Reset group impressions
  await ASRouter.resetGroupsState();
  // Reload the providers
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();
  await SpecialPowers.popPrefEnv();
  CFRPageActions.clearRecommendations();
});
