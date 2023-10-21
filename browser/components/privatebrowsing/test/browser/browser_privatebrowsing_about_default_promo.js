/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PromoInfo = {
  FOCUS: { enabledPref: "browser.promo.focus.enabled" },
  VPN: { enabledPref: "browser.vpn_promo.enabled" },
  PIN: { enabledPref: "browser.promo.pin.enabled" },
  COOKIE_BANNERS: { enabledPref: "browser.promo.cookiebanners.enabled" },
};

const sandbox = sinon.createSandbox();

async function resetState() {
  await Promise.all([
    ASRouter.resetMessageState(),
    ASRouter.resetGroupsState(),
    ASRouter.unblockAll(),
    sandbox.restore(),
  ]);
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
      promoHeader.getAttribute("data-l10n-id"),
      "about-private-browsing-focus-promo-header-c",
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

// Verify that promos are correctly removed if blocked in another tab.
// See handlePromoOnPreload() in aboutPrivateBrowsing.js
add_task(async function test_remove_promo_from_prerendered_tab_if_blocked() {
  await resetState();

  const { win, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function () {
    // container which is present if promo message is not blocked
    const promoContainer = content.document.querySelector(".promo");
    ok(promoContainer, "Focus promo is shown in tab 1");
  });

  // Open a new background tab (tab 2) while the promo message is unblocked
  win.openTrustedLinkIn(win.BROWSER_NEW_TAB_URL, "tabshifted");

  // Block the promo in tab 1
  await SpecialPowers.spawn(tab1, [], async function () {
    content.document.getElementById("dismiss-btn").click();
    await ContentTaskUtils.waitForCondition(() => {
      return !content.document.querySelector(".promo");
    }, "The promo container is removed.");
  });

  // Switch to tab 2, invoking the `visibilitychange` handler in
  // handlePromoOnPreload()
  await BrowserTestUtils.switchTab(win.gBrowser, win.gBrowser.tabs[1]);

  // Verify that the promo has now been removed from tab 2
  await SpecialPowers.spawn(
    win.gBrowser.tabs[1].linkedBrowser,
    [],
    // The timing may be weird in Chaos Mode, so wait for it to be removed
    // instead of a single assertion.
    async function () {
      await ContentTaskUtils.waitForCondition(
        () => !content.document.querySelector(".promo"),
        "Focus promo is not shown in a new tab after being dismissed in another tab"
      );
    }
  );

  await BrowserTestUtils.closeWindow(win);
});

// Test that some default content is rendered while waiting for ASRouter to
// return a message.
add_task(async function test_default_content_deferred_message_load() {
  await resetState();

  let messageRequestedPromiseResolver;
  const messageRequestedPromise = new Promise(resolve => {
    messageRequestedPromiseResolver = resolve;
  });
  let messageReadyPromiseResolver;
  const messageReadyPromise = new Promise(resolve => {
    messageReadyPromiseResolver = resolve;
  });
  // Force ASRouter to "hang" until we resolve the promise so we can test what
  // happens when there is a delay in loading the message.
  const sendMessageStub = sandbox
    .stub(ASRouter, "sendPBNewTabMessage")
    .callsFake(async (...args) => {
      messageRequestedPromiseResolver();
      await messageReadyPromise;
      return sendMessageStub.wrappedMethod.apply(ASRouter, args);
    });

  const { win, tab } = await openAboutPrivateBrowsing();
  await messageRequestedPromise;

  await SpecialPowers.spawn(tab, [], async function () {
    const promoContainer = content.document.querySelector(".promo");
    ok(
      promoContainer && !promoContainer.classList.contains("promo-visible"),
      "Focus promo is hidden but not removed"
    );
    const infoContainer = content.document.querySelector(".info");
    ok(infoContainer && !infoContainer.hidden, "Info container is shown");
    const infoTitle = content.document.getElementById("info-title");
    ok(infoTitle && infoTitle.hidden, "Info title is hidden");
    const infoBody = content.document.getElementById("info-body");
    ok(infoBody, "Info body is shown");
    is(
      infoBody.getAttribute("data-l10n-id"),
      "about-private-browsing-info-description-private-window",
      "Info body has the correct Fluent id"
    );
    await ContentTaskUtils.waitForCondition(
      () => infoBody.textContent,
      "Info body has been translated"
    );
    const infoLink = content.document.getElementById("private-browsing-myths");
    ok(infoLink, "Info link is shown");
    is(
      infoLink.getAttribute("data-l10n-id"),
      "about-private-browsing-learn-more-link",
      "Info link has the correct Fluent id"
    );
    await ContentTaskUtils.waitForCondition(
      () => infoLink.textContent && infoLink.href,
      "Info body has been translated"
    );
  });

  messageReadyPromiseResolver();
  await messageReadyPromise;

  await SpecialPowers.spawn(tab, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      const promoContainer = content.document.querySelector(".promo");
      return promoContainer?.classList.contains("promo-visible");
    }, "The promo container is shown.");
  });

  await BrowserTestUtils.closeWindow(win);
});
