/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests to ensure that sending or canceling reports with
 * the Send and Cancel buttons work (as well as the Okay button)
 */

"use strict";

add_common_setup();

function pressKeyAndAwait(event, key, config = {}) {
  const win = config.window || window;
  if (!event.then) {
    event = BrowserTestUtils.waitForEvent(win, event, config.timeout || 200);
  }
  EventUtils.synthesizeKey(key, config, win);
  return event;
}

async function pressKeyAndGetFocus(key, config = {}) {
  return (await pressKeyAndAwait("focus", key, config)).target;
}

async function ensureTabOrder(order, win = window) {
  const config = { window: win };
  for (let matches of order) {
    // We need to tab through all elements in each match array in any order
    if (!Array.isArray(matches)) {
      matches = [matches];
    }
    let matchesLeft = matches.length;
    while (matchesLeft--) {
      const target = await pressKeyAndGetFocus("VK_TAB", config);
      let foundMatch = false;
      for (const [i, selector] of matches.entries()) {
        foundMatch = selector && target.matches(selector);
        if (foundMatch) {
          matches[i] = "";
          break;
        }
      }
      ok(
        foundMatch,
        `Expected [${matches}] next, got id=${target.id}, class=${target.className}, ${target}`
      );
      if (!foundMatch) {
        return false;
      }
    }
  }
  return true;
}

async function tabTo(match, win = window) {
  const config = { window: win };
  let initial = await pressKeyAndGetFocus("VK_TAB", config);
  let target = initial;
  do {
    if (target.matches(match)) {
      return target;
    }
    target = await pressKeyAndGetFocus("VK_TAB", config);
  } while (target && target !== initial);
  return undefined;
}

async function ensureExpectedTabOrder(
  expectBackButton,
  expectReason,
  expectSendMoreInfo
) {
  const order = [];
  if (expectBackButton) {
    order.push(".subviewbutton-back");
  }
  order.push("#report-broken-site-popup-url");
  if (expectReason) {
    order.push("#report-broken-site-popup-reason");
  }
  order.push("#report-broken-site-popup-description");
  if (expectSendMoreInfo) {
    order.push("#report-broken-site-popup-send-more-info-link");
  }
  // moz-button-groups swap the order of buttons to follow
  // platform conventions, so the order of send/cancel will vary.
  order.push([
    "#report-broken-site-popup-cancel-button",
    "#report-broken-site-popup-send-button",
  ]);
  order.push(order[0]); // check that we've cycled back
  return ensureTabOrder(order);
}

async function testTabOrder(menu) {
  ensureReasonDisabled();
  ensureSendMoreInfoDisabled();

  const { showsBackButton } = menu;

  let rbs = await menu.openReportBrokenSite();
  await ensureExpectedTabOrder(showsBackButton, false, false);
  await rbs.close();

  ensureSendMoreInfoEnabled();
  rbs = await menu.openReportBrokenSite();
  await ensureExpectedTabOrder(showsBackButton, false, true);
  await rbs.close();

  ensureReasonOptional();
  rbs = await menu.openReportBrokenSite();
  await ensureExpectedTabOrder(showsBackButton, true, true);
  await rbs.close();

  ensureReasonRequired();
  rbs = await menu.openReportBrokenSite();
  await ensureExpectedTabOrder(showsBackButton, true, true);
  await rbs.close();
  rbs = await menu.openReportBrokenSite();
  rbs.chooseReason("slow");
  await ensureExpectedTabOrder(showsBackButton, true, true);
  await rbs.clickCancel();

  ensureSendMoreInfoDisabled();
  rbs = await menu.openReportBrokenSite();
  await ensureExpectedTabOrder(showsBackButton, true, false);
  await rbs.close();
  rbs = await menu.openReportBrokenSite();
  rbs.chooseReason("slow");
  await ensureExpectedTabOrder(showsBackButton, true, false);
  await rbs.clickCancel();

  ensureReasonOptional();
  rbs = await menu.openReportBrokenSite();
  await ensureExpectedTabOrder(showsBackButton, true, false);
  await rbs.close();

  ensureReasonDisabled();
  rbs = await menu.openReportBrokenSite();
  await ensureExpectedTabOrder(showsBackButton, false, false);
  await rbs.close();
}

add_task(async function testTabOrdering() {
  ensureReportBrokenSitePreffedOn();
  ensureSendMoreInfoEnabled();

  await BrowserTestUtils.withNewTab(
    REPORTABLE_PAGE_URL,
    async function (browser) {
      await testTabOrder(AppMenu());
      await testTabOrder(ProtectionsPanel());
      await testTabOrder(HelpMenu());
    }
  );
});

async function testPressingKey(key, tabToMatch, makePromise, followUp) {
  await BrowserTestUtils.withNewTab(
    REPORTABLE_PAGE_URL,
    async function (browser) {
      for (const menu of [AppMenu(), ProtectionsPanel(), HelpMenu()]) {
        const rbs = await menu.openReportBrokenSite();
        const promise = makePromise(rbs);
        if (tabToMatch) {
          if (await tabTo(tabToMatch)) {
            await pressKeyAndAwait(promise, key);
            followUp && (await followUp(rbs));
            await rbs.close();
            ok(true, `was able to activate ${tabToMatch} with keyboard`);
          } else {
            await rbs.close();
            ok(false, `could not tab to ${tabToMatch}`);
          }
        } else {
          await pressKeyAndAwait(promise, key);
          followUp && (await followUp(rbs));
          await rbs.close();
          ok(true, `was able to use keyboard`);
        }
      }
    }
  );
}

add_task(async function testSendMoreInfo() {
  ensureReportBrokenSitePreffedOn();
  ensureSendMoreInfoEnabled();
  await testPressingKey(
    "KEY_Enter",
    "#report-broken-site-popup-send-more-info-link",
    rbs => rbs.waitForSendMoreInfoTab(),
    () => gBrowser.removeCurrentTab()
  );
});

add_task(async function testCancel() {
  ensureReportBrokenSitePreffedOn();
  await testPressingKey(
    "KEY_Enter",
    "#report-broken-site-popup-cancel-button",
    rbs => BrowserTestUtils.waitForEvent(rbs.mainView, "ViewHiding")
  );
});

add_task(async function testSendAndOkay() {
  ensureReportBrokenSitePreffedOn();
  await testPressingKey(
    "KEY_Enter",
    "#report-broken-site-popup-send-button",
    rbs => BrowserTestUtils.waitForEvent(rbs.sentView, "ViewShown"),
    async rbs => {
      await tabTo("#report-broken-site-popup-okay-button");
      const promise = BrowserTestUtils.waitForEvent(rbs.sentView, "ViewHiding");
      await pressKeyAndAwait(promise, "KEY_Enter");
    }
  );
});

add_task(async function testESCOnMain() {
  ensureReportBrokenSitePreffedOn();
  await testPressingKey("KEY_Escape", undefined, rbs =>
    BrowserTestUtils.waitForEvent(rbs.mainView, "ViewHiding")
  );
});

add_task(async function testESCOnSent() {
  ensureReportBrokenSitePreffedOn();
  await testPressingKey(
    "KEY_Enter",
    "#report-broken-site-popup-send-button",
    rbs => BrowserTestUtils.waitForEvent(rbs.sentView, "ViewShown"),
    async rbs => {
      const promise = BrowserTestUtils.waitForEvent(rbs.sentView, "ViewHiding");
      await pressKeyAndAwait(promise, "KEY_Escape");
    }
  );
});

add_task(async function testBackButtons() {
  ensureReportBrokenSitePreffedOn();
  await BrowserTestUtils.withNewTab(
    REPORTABLE_PAGE_URL,
    async function (browser) {
      for (const menu of [AppMenu(), ProtectionsPanel()]) {
        await menu.openReportBrokenSite();
        await tabTo("#report-broken-site-popup-mainView .subviewbutton-back");
        const promise = BrowserTestUtils.waitForEvent(menu.popup, "ViewShown");
        await pressKeyAndAwait(promise, "KEY_Enter");
        menu.close();
      }
    }
  );
});
