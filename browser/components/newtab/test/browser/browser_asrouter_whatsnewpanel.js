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
  // Force the WNPanel provider cache to 0 by modifying updateCycleInMs
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel",
        `{"id":"whats-new-panel","enabled":true,"type":"remote-settings","bucket":"whats-new-panel","updateCycleInMs":0}`,
      ],
    ],
  });
  const msgs = (await PanelTestProvider.getMessages()).filter(
    ({ template }) => template === "whatsnew_panel_message"
  );
  const initialMessageCount = ASRouter.state.messages.length;
  const client = RemoteSettings("whats-new-panel");
  await client.db.clear();
  for (const record of msgs) {
    await client.db.create(
      // Modify targeting to ensure the messages always show up
      { ...record, targeting: "true" }
    );
  }
  await client.db.saveLastModified(42); // Prevent from loading JSON dump.

  const whatsNewBtn = document.getElementById("appMenu-whatsnew-button");
  Assert.equal(whatsNewBtn.hidden, true, "What's New btn doesn't exist");

  const mainView = document.getElementById("appMenu-mainView");
  UITour.showMenu(window, "appMenu");
  await BrowserTestUtils.waitForEvent(
    mainView,
    "ViewShown",
    "Panel did not open"
  );

  // Reload the provider
  await ASRouter._updateMessageProviders();
  // Wait to load the WNPanel messages
  await BrowserTestUtils.waitForCondition(async () => {
    await ASRouter.loadMessagesFromAllProviders();
    return ASRouter.state.messages.length > initialMessageCount;
  }, "Messages did not load");
  await ToolbarPanelHub.enableAppmenuButton();

  Assert.equal(mainView.hidden, false, "Panel is visible");
  await BrowserTestUtils.waitForCondition(
    () => !whatsNewBtn.hidden,
    "What's new menu entry did not become visible"
  );
  Assert.equal(whatsNewBtn.hidden, false, "What's New btn is visible");

  // Show the What's New Messages
  whatsNewBtn.click();

  await BrowserTestUtils.waitForCondition(
    () => document.getElementById("PanelUI-whatsNew-message-container"),
    "The message container did not show"
  );
  await BrowserTestUtils.waitForCondition(
    () =>
      document.querySelectorAll(
        "#PanelUI-whatsNew-message-container .whatsNew-message"
      ).length === msgs.length,
    "The message container was not populated with the expected number of msgs"
  );

  await BrowserTestUtils.waitForCondition(
    () =>
      document.querySelector(
        "#PanelUI-whatsNew-message-container .whatsNew-message-body remote-text"
      ).shadowRoot.innerHTML,
    "Ensure messages have content"
  );

  UITour.hideMenu(window, "appMenu");
  // Clean up and remove messages
  ToolbarPanelHub.disableAppmenuButton();
  await client.db.clear();
  // Wait to reset the WNPanel messages from state
  const previousMessageCount = ASRouter.state.messages.length;
  await BrowserTestUtils.waitForCondition(async () => {
    await ASRouter.loadMessagesFromAllProviders();
    return ASRouter.state.messages.length < previousMessageCount;
  }, "WNPanel messages should have been removed");
  await SpecialPowers.popPrefEnv();
  // Reload the provider
  await ASRouter._updateMessageProviders();
});
