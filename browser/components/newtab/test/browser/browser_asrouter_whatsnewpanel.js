const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);
const { ToolbarPanelHub } = ChromeUtils.import(
  "resource://activity-stream/lib/ToolbarPanelHub.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

add_task(async function test_with_rs_messages() {
  const msgs = (await PanelTestProvider.getMessages()).filter(
    ({ template }) => template === "whatsnew_panel_message"
  );
  const initialMessageCount = ASRouter.state.messages.length;
  const client = RemoteSettings("whats-new-panel");
  const collection = await client.openCollection();
  await collection.clear();
  for (const record of msgs) {
    await collection.create(
      // Modify targeting to ensure the messages always show up
      { ...record, targeting: "true" },
      { useRecordId: true }
    );
  }
  await collection.db.saveLastModified(42); // Prevent from loading JSON dump.

  const whatsNewBtn = document.getElementById("appMenu-whatsnew-button");
  Assert.equal(whatsNewBtn.hidden, true, "What's New btn doesn't exist");

  // Force the WNPanel provider cache to 0 by modifying updateCycleInMs
  Services.prefs.setStringPref(
    "browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel",
    `{"id":"whats-new-panel","enabled":true,"type":"remote-settings","bucket":"whats-new-panel","updateCycleInMs":0}`
  );
  // Reload the provider
  ASRouter._updateMessageProviders();
  // Wait to load the WNPanel messages
  await BrowserTestUtils.waitForCondition(async () => {
    await ASRouter.loadMessagesFromAllProviders();
    return ASRouter.state.messages.length === msgs.length + initialMessageCount;
  });
  await ToolbarPanelHub.enableAppmenuButton();

  const mainView = document.getElementById("appMenu-mainView");
  UITour.showMenu(window, "appMenu");
  await BrowserTestUtils.waitForEvent(mainView, "ViewShown");

  Assert.equal(mainView.hidden, false, "Panel is visible");
  await BrowserTestUtils.waitForCondition(() => !whatsNewBtn.hidden);
  Assert.equal(whatsNewBtn.hidden, false, "What's New btn is visible");

  // Show the What's New Messages
  whatsNewBtn.click();

  await BrowserTestUtils.waitForCondition(() =>
    document.getElementById("PanelUI-whatsNew-message-container")
  );
  await BrowserTestUtils.waitForCondition(
    () =>
      document.querySelectorAll(
        "#PanelUI-whatsNew-message-container .whatsNew-message"
      ).length === msgs.length
  );

  UITour.hideMenu(window, "appMenu");
  // Clean up and remove messages
  ToolbarPanelHub.disableAppmenuButton();
  collection.clear();
  // Wait to reset the WNPanel messages from state
  await BrowserTestUtils.waitForCondition(async () => {
    await ASRouter.loadMessagesFromAllProviders();
    return ASRouter.state.messages.length === initialMessageCount;
  });
  Services.prefs.clearUserPref(
    "browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel"
  );
  // Reload the provider
  ASRouter._updateMessageProviders();
});
