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
        `{"id":"cfr","enabled":true,"type":"remote-settings","bucket":"cfr","categories":["cfrAddons","cfrFeatures"],"updateCycleInMs":0}`,
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
  await client.db.clear();
  await client.db.create(testMessage);
  await client.db.saveLastModified(42); // Prevent from loading JSON dump.

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
 * Test group frequency capping.
 * Message has a lifetime frequency of 3 but it's group has a lifetime frequency
 * of 2. It should only show up twice.
 * We update the provider to remove any daily limitations so it should show up
 * on every new tab load.
 */
add_task(async function test_heartbeat_tactic_2() {
  const TEST_URL = "http://example.com";

  // Force the WNPanel provider cache to 0 by modifying updateCycleInMs
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
    userPreferences: [],
    frequency: { lifetime: 2 },
  };
  const client = RemoteSettings("message-groups");
  await client.db.clear();
  await client.db.create(groupConfiguration);
  await client.db.saveLastModified(42); // Prevent from loading JSON dump.

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
  Assert.ok(chiclet, "CFR chiclet element found (tab1)");
  await BrowserTestUtils.waitForCondition(
    () => !chiclet.hidden,
    "Chiclet should be visible (tab1)"
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  await BrowserTestUtils.loadURI(tab2.linkedBrowser, TEST_URL);

  Assert.ok(chiclet, "CFR chiclet element found (tab2)");
  await BrowserTestUtils.waitForCondition(
    () => !chiclet.hidden,
    "Chiclet should be visible (tab2)"
  );

  // Wait for the message to become blocked (frequency limit reached)
  const msg = ASRouter.state.messages.find(m =>
    m.groups.includes("messaging-experiments")
  );
  Assert.ok(msg, "Message found");
  await BrowserTestUtils.waitForCondition(
    () => !ASRouter.isBelowFrequencyCaps(msg),
    "Message frequency limit should be reached"
  );

  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  await BrowserTestUtils.loadURI(tab3.linkedBrowser, TEST_URL);

  await BrowserTestUtils.waitForCondition(
    () => chiclet.hidden,
    "Heartbeat button should be hidden"
  );

  info("Cleanup");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  await client.db.clear();
  // Reset group impressions
  await ASRouter.setGroupState({ id: "messaging-experiments", value: true });
  await ASRouter.setGroupState({ id: "cfr", value: true });
  // Reload the providers
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();
  await SpecialPowers.popPrefEnv();
});
