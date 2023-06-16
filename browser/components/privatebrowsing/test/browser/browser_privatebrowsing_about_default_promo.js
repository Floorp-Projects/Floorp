/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PromoInfo = {
  FOCUS: { enabledPref: "browser.promo.focus.enabled" },
  VPN: { enabledPref: "browser.vpn_promo.enabled" },
  PIN: { enabledPref: "browser.promo.pin.enabled" },
  COOKIE_BANNERS: { enabledPref: "browser.promo.cookiebanners.enabled" },
};

async function resetState() {
  await Promise.all([ASRouter.resetMessageState(), ASRouter.unblockAll()]);
}

add_setup(async function () {
  registerCleanupFunction(resetState);
  await resetState();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.promo.pin.enabled", false]],
  });
  await ASRouter.onPrefChange();
});

add_task(async function test_privatebrowsing_asrouter_messages_state() {
  await resetState();
  let pinPromoMessage = ASRouter.state.messages.find(
    m => m.id === "PB_NEWTAB_PIN_PROMO"
  );
  Assert.ok(pinPromoMessage, "Pin Promo message found");

  const initialMessages = JSON.parse(JSON.stringify(ASRouter.state.messages));

  let { win, tab } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab, [], async function () {
    const promoContainer = content.document.querySelector(".promo");
    ok(promoContainer, "Focus promo is shown");
  });

  Assert.equal(
    ASRouter.state.messages.filter(m => m.id === "PB_NEWTAB_PIN_PROMO").length,
    0,
    "Pin Promo message removed from state when Promotype Pin is disabled"
  );

  for (let msg of initialMessages) {
    let shouldPersist =
      msg.template !== "pb_newtab" ||
      Services.prefs.getBoolPref(
        PromoInfo[msg.content?.promoType]?.enabledPref,
        true
      );
    Assert.equal(
      !!ASRouter.state.messages.find(m => m.id === msg.id),
      shouldPersist,
      shouldPersist
        ? "Message persists in ASRouter state"
        : "Promo message with disabled promoType removed from ASRouter state"
    );
  }
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_default_promo() {
  await resetState();

  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function () {
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

  await SpecialPowers.spawn(tab4, [], async function () {
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
  await resetState();

  const { win, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function () {
    const promoContainer = content.document.querySelector(".promo"); // container which is present if promo message is not blocked
    ok(promoContainer, "Focus promo is shown in a new tab");
    content.document.getElementById("dismiss-btn").click();
    await ContentTaskUtils.waitForCondition(() => {
      return !content.document.querySelector(".promo");
    }, "The promo container is removed.");
  });

  let newTabStartPromise = new Promise(resolve => {
    async function observer(subject) {
      Services.obs.removeObserver(observer, "browser-open-newtab-start");
      resolve(subject.wrappedJSObject);
    }
    Services.obs.addObserver(observer, "browser-open-newtab-start");
  });
  let newtabShown = BrowserTestUtils.waitForCondition(
    () => win.gBrowser.currentURI.spec == "about:privatebrowsing",
    `Should open correct url about:privatebrowsing.`
  );
  win.BrowserOpenTab();
  const tab2 = await newTabStartPromise;
  await newtabShown;
  await BrowserTestUtils.switchTab(win.gBrowser, tab2);

  await SpecialPowers.spawn(tab2, [], async function () {
    await ContentTaskUtils.waitForCondition(
      () => !content.document.querySelector(".promo"),
      "Focus promo is not shown in a new tab after being dismissed in another tab"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});
