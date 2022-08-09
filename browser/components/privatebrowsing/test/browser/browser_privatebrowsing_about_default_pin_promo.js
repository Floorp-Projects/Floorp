/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

add_setup(async function() {
  ASRouter.resetMessageState();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.promo.pin.enabled", true]],
  });
  await ASRouter.onPrefChange();
});

add_task(async function test_pin_promo() {
  const sandbox = sinon.createSandbox();
  // Stub out the doesAppNeedPin to true so that Pin Promo targeting evaluates true
  const { ShellService } = ChromeUtils.import(
    "resource:///modules/ShellService.jsm"
  );
  sandbox
    .stub(ShellService, "doesAppNeedPin")
    .withArgs(true)
    .returns(true);
  registerCleanupFunction(async () => {
    ASRouter.resetMessageState();
    sandbox.restore();
  });
  let { win: win1, tab: tab1 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab1, [], async function() {
    const promoContainer = content.document.querySelector(".promo");
    const promoHeader = content.document.getElementById("promo-header");

    ok(promoContainer, "Pin promo is shown");
    is(
      promoHeader.textContent,
      "Private browsing freedom in one click",
      "Correct default values are shown"
    );
  });

  let { win: win2 } = await openTabAndWaitForRender();
  let { win: win3 } = await openTabAndWaitForRender();
  let { win: win4, tab: tab4 } = await openTabAndWaitForRender();

  await SpecialPowers.spawn(tab4, [], async function() {
    is(
      content.document.getElementById(".private-browsing-promo-link"),
      null,
      "should no longer render the promo after 3 impressions"
    );
  });

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win3);
  await BrowserTestUtils.closeWindow(win4);
});
