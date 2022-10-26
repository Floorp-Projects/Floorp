/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

add_setup(async function() {
  ASRouter.resetMessageState();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.promo.pin.enabled", false]],
  });
  await ASRouter.onPrefChange();
});

add_task(async function test_privatebrowsing_asrouter_messages_state() {
  ASRouter.resetMessageState();
  const initialMsgCount = ASRouter.state.messages.length;
  let pinPromoMessage = ASRouter.state.messages.find(
    m => m.id === "PB_NEWTAB_PIN_PROMO"
  );

  Assert.ok(pinPromoMessage, "Pin Promo message found");
  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function() {
    const promoContainer = content.document.querySelector(".promo");
    ok(promoContainer, "Focus promo is shown");
  });

  Assert.equal(
    ASRouter.state.messages.filter(m => m.id === "PB_NEWTAB_PIN_PROMO").length,
    0,
    "Pin Promo message removed from state when Promotype Pin is disabled"
  );
  Assert.equal(
    ASRouter.state.messages.length,
    initialMsgCount - 1,
    "ASRouter state browsing messages persists"
  );
  await BrowserTestUtils.closeWindow(win1);
});

add_task(async function test_default_promo() {
  ASRouter.resetMessageState();

  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function() {
    const promoContainer = content.document.querySelector(".promo"); // container which is present if promo is enabled and should show
    const promoHeader = content.document.getElementById("promo-header");

    ok(promoContainer, "Focus promo is shown");
    is(
      promoHeader.textContent,
      "Next-level privacy on mobile",
      "Correct default values are shown"
    );
  });

  let { win: win2 } = await openTabAndWaitForRender();
  let { win: win3 } = await openTabAndWaitForRender();

  let { win: win4, tab: tab4 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab4, [], async function() {
    is(
      content.document.querySelector(".promo button"),
      null,
      "should no longer render the promo after 3 impressions"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win3);
  await BrowserTestUtils.closeWindow(win4);
});

add_task(async function test_remove_promo_from_prerendered_tab_if_blocked() {
  ASRouter.resetMessageState();

  const { win, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function() {
    const promoContainer = content.document.querySelector(".promo"); // container which is present if promo message is not blocked
    ok(promoContainer, "Focus promo is shown in a new tab");
    content.document.getElementById("dismiss-btn").click();
    await ContentTaskUtils.waitForCondition(() => {
      return !content.document.querySelector(".promo");
    }, "The promo container is removed.");
  });

  win.BrowserOpenTab();
  await BrowserTestUtils.switchTab(win.gBrowser, win.gBrowser.tabs[1]);
  await SimpleTest.promiseFocus(win.gBrowser.selectedBrowser);
  const tab2 = win.gBrowser.selectedBrowser;

  await SpecialPowers.spawn(tab2, [], async function() {
    const promoContainer = content.document.querySelector(".promo"); // container which is not present if promo message is blocked
    ok(
      !promoContainer,
      "Focus promo is not shown in a new tab after being dismissed in another tab"
    );
  });

  await ASRouter.unblockAll();
  await BrowserTestUtils.closeWindow(win);
});
