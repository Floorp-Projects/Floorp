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
  await client.db.importChanges({}, Date.now(), [testMessage], { clear: true });

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
 * Test group user preferences.
 * Group is enabled if both user preferences are enabled.
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
    userPreferences: ["browser.userPreference.messaging-experiments"],
  };
  const client = RemoteSettings("message-groups");
  await client.db.importChanges({}, Date.now(), [groupConfiguration], {
    clear: true,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.message-groups",
        `{"id":"message-groups","enabled":true,"type":"remote-settings","collection":"message-groups","updateCycleInMs":0}`,
      ],
      ["browser.userPreference.messaging-experiments", true],
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
  Assert.ok(ASRouter.isUnblockedMessage(msg), "Message is unblocked");

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  BrowserTestUtils.startLoadingURIString(tab1.linkedBrowser, TEST_URL);

  let chiclet = document.getElementById("contextual-feature-recommendation");
  Assert.ok(chiclet, "CFR chiclet element found");
  await BrowserTestUtils.waitForCondition(
    () => !chiclet.hidden,
    "Chiclet should be visible (userprefs enabled)"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.userPreference.messaging-experiments", false]],
  });

  await BrowserTestUtils.waitForCondition(
    () =>
      ASRouter.state.groups.find(
        g => g.id === groupConfiguration.id && !g.enable
      ),
    "Wait for group config to load"
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  BrowserTestUtils.startLoadingURIString(tab2.linkedBrowser, TEST_URL);

  await BrowserTestUtils.waitForCondition(
    () => chiclet.hidden,
    "Heartbeat button should not be visible (userprefs disabled)"
  );

  info("Cleanup");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await client.db.clear();
  // Reset group impressions
  await ASRouter.resetGroupsState();
  // Reload the providers
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();
  await SpecialPowers.popPrefEnv();
  CFRPageActions.clearRecommendations();
});
