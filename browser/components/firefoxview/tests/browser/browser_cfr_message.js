const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);

const { SpecialMessageActions } = ChromeUtils.import(
  "resource://messaging-system/lib/SpecialMessageActions.jsm"
);

add_task(async function cfr_firefoxview_should_show() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.view-count", 0]],
  });

  let cfrSpy = sinon.spy(ASRouter, "routeCFRMessage");
  let specialMessageActionsSpy = sinon.spy(
    SpecialMessageActions,
    "handleAction"
  );
  registerCleanupFunction(() => {
    cfrSpy.restore();
    specialMessageActionsSpy.restore();
    ASRouter.resetMessageState();
    ASRouter.unblockMessageById("CFR_FIREFOX_VIEW");
    ASRouterTriggerListeners.get("nthTabClosed").uninit();
  });

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  const showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown",
    target => {
      return target;
    }
  );
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);

  await showPanel;

  Assert.equal(cfrSpy.lastCall.args[0].id, "CFR_FIREFOX_VIEW");

  const notification = document.querySelector(
    "#contextual-feature-recommendation-notification"
  );

  Assert.ok(notification);
  Assert.ok(document.querySelector(".popup-notification-primary-button"));

  Assert.ok(document.querySelector(".popup-notification-secondary-button"));

  await notification.button.click();

  Assert.equal(
    specialMessageActionsSpy.firstCall.args[0].type,
    "OPEN_FIREFOX_VIEW"
  );
  await SpecialPowers.popPrefEnv();

  closeFirefoxViewTab(window);
});
