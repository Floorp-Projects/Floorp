const { PanelTestProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/PanelTestProvider.sys.mjs"
);
const { MomentsPageHub } = ChromeUtils.import(
  "resource://activity-stream/lib/MomentsPageHub.jsm"
);
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const HOMEPAGE_OVERRIDE_PREF = "browser.startup.homepage_override.once";

add_task(async function test_with_rs_messages() {
  // Force the WNPanel provider cache to 0 by modifying updateCycleInMs
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel",
        `{"id":"cfr","enabled":true,"type":"remote-settings","collection":"cfr","updateCycleInMs":0}`,
      ],
    ],
  });
  const [msg] = (await PanelTestProvider.getMessages()).filter(
    ({ template }) => template === "update_action"
  );
  const initialMessageCount = ASRouter.state.messages.length;
  const client = RemoteSettings("cfr");
  await client.db.importChanges(
    {},
    Date.now(),
    [
      {
        // Modify targeting and randomize message name to work around the message
        // getting blocked (for --verify)
        ...msg,
        id: `MOMENTS_MOCHITEST_${Date.now()}`,
        targeting: "true",
      },
    ],
    { clear: true }
  );
  // Reload the provider
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();
  // Wait to load the WNPanel messages
  await BrowserTestUtils.waitForCondition(
    () => ASRouter.state.messages.length > initialMessageCount,
    "Messages did not load"
  );

  await MomentsPageHub.messageRequest({
    triggerId: "momentsUpdate",
    template: "update_action",
  });

  await BrowserTestUtils.waitForCondition(() => {
    return Services.prefs.getStringPref(HOMEPAGE_OVERRIDE_PREF, "").length;
  }, "Pref value was not set");

  let value = Services.prefs.getStringPref(HOMEPAGE_OVERRIDE_PREF, "");
  is(JSON.parse(value).url, msg.content.action.data.url, "Correct value set");

  // Insert a new message and test that priority override works as expected
  msg.content.action.data.url = "https://www.mozilla.org/#mochitest";
  await client.db.create(
    // Modify targeting to ensure the messages always show up
    {
      ...msg,
      id: `MOMENTS_MOCHITEST_${Date.now()}`,
      priority: 2,
      targeting: "true",
    }
  );

  // Reset so we can `await` for the pref value to be set again
  Services.prefs.clearUserPref(HOMEPAGE_OVERRIDE_PREF);

  let prevLength = ASRouter.state.messages.length;
  // Wait to load the messages
  await ASRouter.loadMessagesFromAllProviders();
  await BrowserTestUtils.waitForCondition(
    () => ASRouter.state.messages.length > prevLength,
    "Messages did not load"
  );

  await MomentsPageHub.messageRequest({
    triggerId: "momentsUpdate",
    template: "update_action",
  });

  await BrowserTestUtils.waitForCondition(() => {
    return Services.prefs.getStringPref(HOMEPAGE_OVERRIDE_PREF, "").length;
  }, "Pref value was not set");

  value = Services.prefs.getStringPref(HOMEPAGE_OVERRIDE_PREF, "");
  is(
    JSON.parse(value).url,
    msg.content.action.data.url,
    "Correct value set for higher priority message"
  );

  await client.db.clear();
  // Wait to reset the WNPanel messages from state
  const previousMessageCount = ASRouter.state.messages.length;
  await ASRouter.loadMessagesFromAllProviders();
  await BrowserTestUtils.waitForCondition(
    () => ASRouter.state.messages.length < previousMessageCount,
    "ASRouter messages should have been removed"
  );
  await SpecialPowers.popPrefEnv();
  // Reload the provider
  await ASRouter._updateMessageProviders();
});
