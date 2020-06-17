const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);

/**
 * Load and modify a message for the test.
 */
add_task(async function setup() {
  // Force the WNPanel provider cache to 0 by modifying updateCycleInMs
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.cfr",
        `{"id":"cfr","enabled":true,"type":"remote-settings","bucket":"cfr","updateCycleInMs":0}`,
      ],
    ],
  });

  const initialMsgCount = ASRouter.state.messages.length;

  const heartbeatMsg = CFRMessageProvider.getMessages().find(
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
  await client.db.importChanges({}, 42, [testMessage], { clear: true });

  // Reload the providers
  await BrowserTestUtils.waitForCondition(async () => {
    await ASRouter._updateMessageProviders();
    await ASRouter.loadMessagesFromAllProviders();
    return ASRouter.state.messages.length > initialMsgCount;
  }, "Should load the extra heartbeat message");

  BrowserTestUtils.waitForCondition(
    () => ASRouter.state.messages.find(m => m.id === testMessage.id),
    "Wait to load the message"
  );

  const msg = ASRouter.state.messages.find(m => m.id === testMessage.id);
  Assert.equal(msg.targeting, "true");
  Assert.equal(msg.groups[0], "messaging-experiments");
  Assert.ok(ASRouter.isUnblockedMessage(msg), "Message is unblocked");

  registerCleanupFunction(async () => {
    await client.db.clear();
    // Reload the providers
    await BrowserTestUtils.waitForCondition(async () => {
      await ASRouter._updateMessageProviders();
      await ASRouter.loadMessagesFromAllProviders();
      return ASRouter.state.messages.length === initialMsgCount;
    }, "Should reset messages");
    await SpecialPowers.popPrefEnv();
  });
});

/**
 * Test group user preferences.
 * Group is enabled if both user preferences are enabled.
 */
add_task(async function test_heartbeat_tactic_2() {
  const TEST_URL = "http://example.com";

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.message-groups",
        `{"id":"message-groups","enabled":true,"type":"remote-settings","bucket":"message-groups","updateCycleInMs":0}`,
      ],
    ],
  });
  const groupConfiguration = {
    id: "messaging-experiments",
    enabled: true,
    userPreferences: ["browser.userPreference.messaging-experiments"],
  };
  const client = RemoteSettings("message-groups");
  await client.db.importChanges({}, 42, [groupConfiguration], { clear: true });

  // Reload the providers
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();

  await BrowserTestUtils.waitForCondition(
    () => ASRouter.state.groups.find(g => g.id === groupConfiguration.id),
    "Wait for group config to load"
  );
  let groupState = ASRouter.state.groups.find(
    g => g.id === groupConfiguration.id
  );
  Assert.ok(groupState, "Group config found");
  Assert.ok(groupState.enabled, "Group is enabled");

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  await BrowserTestUtils.loadURI(tab1.linkedBrowser, TEST_URL);

  let chiclet = document.getElementById("contextual-feature-recommendation");
  Assert.ok(chiclet, "CFR chiclet element found");
  await BrowserTestUtils.waitForCondition(
    () => !chiclet.hidden,
    "Chiclet should be visible (userprefs enabled)"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.userPreference.messaging-experiments", false]],
  });

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  await BrowserTestUtils.loadURI(tab2.linkedBrowser, TEST_URL);

  await BrowserTestUtils.waitForCondition(
    () => chiclet.hidden,
    "Heartbeat button should not be visible (userprefs disabled)"
  );

  info("Cleanup");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await client.db.clear();
  // Reset group impressions
  await ASRouter.setGroupState({ id: "messaging-experiments", value: true });
  await ASRouter.setGroupState({ id: "cfr", value: true });
  // Reload the providers
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();
  await SpecialPowers.popPrefEnv();
});
