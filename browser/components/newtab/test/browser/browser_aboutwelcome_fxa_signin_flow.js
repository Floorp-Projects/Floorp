/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);

const TEST_ROOT = "https://example.com/";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.remote.root", TEST_ROOT]],
  });
});

/**
 * Tests that the AWFxASignInTabFlow method exposed to about:welcome
 * resolves to `true` and closes the FxA sign-in tab if sign-in is
 * successful.
 */
add_task(async function test_fxa_sign_success() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWFxASignInTabFlow();
    });
    let fxaTab = await fxaTabPromise;
    let fxaTabClosing = BrowserTestUtils.waitForTabClosing(fxaTab);

    // We'll fake-out the UIState being in the STATUS_SIGNED_IN status
    // and not test the actual FxA sign-in mechanism.
    sandbox.stub(UIState, "get").returns({
      status: UIState.STATUS_SIGNED_IN,
      syncEnabled: true,
      email: "email@example.com",
    });

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    await fxaTabClosing;
    Assert.ok(true, "FxA tab automatically closed.");
    let result = await resultPromise;
    Assert.ok(result, "AWFxASignInTabFlow should have resolved to true");
  });

  sandbox.restore();
});

/**
 * Tests that the AWFxASignInTabFlow method can disable the autoclose
 * behavior with a data.autoClose parameter.
 */
add_task(async function test_fxa_sign_success_no_autoclose() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWFxASignInTabFlow({ autoClose: false });
    });
    let fxaTab = await fxaTabPromise;

    // We'll fake-out the UIState being in the STATUS_SIGNED_IN status
    // and not test the actual FxA sign-in mechanism.
    sandbox.stub(UIState, "get").returns({
      status: UIState.STATUS_SIGNED_IN,
      syncEnabled: true,
      email: "email@example.com",
    });

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    let result = await resultPromise;
    Assert.ok(result, "AWFxASignInTabFlow should have resolved to true");
    Assert.ok(!fxaTab.closing, "FxA tab was not asked to close.");
    BrowserTestUtils.removeTab(fxaTab);
  });

  sandbox.restore();
});

/**
 * Tests that the AWFxASignInTabFlow method exposed to about:welcome
 * resolves to `false` if the tab closes before sign-in completes.
 */
add_task(async function test_fxa_signin_aborted() {
  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWFxASignInTabFlow();
    });
    let fxaTab = await fxaTabPromise;
    Assert.ok(!fxaTab.closing, "FxA tab was not asked to close yet.");

    BrowserTestUtils.removeTab(fxaTab);
    let result = await resultPromise;
    Assert.ok(!result, "AWFxASignInTabFlow should have resolved to false");
  });
});
