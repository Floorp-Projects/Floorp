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
 * Tests that the FXA_SIGNIN_FLOW special action resolves to `true` and
 * closes the FxA sign-in tab if sign-in is successful.
 */
add_task(async function test_fxa_sign_success() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
      });
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
    Assert.ok(result, "FXA_SIGNIN_FLOW action's result should be true");
  });

  sandbox.restore();
});

/**
 * Tests that the FXA_SIGNIN_FLOW action's data.autoClose parameter can
 * disable the autoclose behavior.
 */
add_task(async function test_fxa_sign_success_no_autoclose() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
        data: { autoClose: false },
      });
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
    Assert.ok(result, "FXA_SIGNIN_FLOW should have resolved to true");
    Assert.ok(!fxaTab.closing, "FxA tab was not asked to close.");
    BrowserTestUtils.removeTab(fxaTab);
  });

  sandbox.restore();
});

/**
 * Tests that the FXA_SIGNIN_FLOW action resolves to `false` if the tab
 * closes before sign-in completes.
 */
add_task(async function test_fxa_signin_aborted() {
  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
      });
    });
    let fxaTab = await fxaTabPromise;
    Assert.ok(!fxaTab.closing, "FxA tab was not asked to close yet.");

    BrowserTestUtils.removeTab(fxaTab);
    let result = await resultPromise;
    Assert.ok(!result, "FXA_SIGNIN_FLOW action's result should be false");
  });
});

/**
 * Tests that the FXA_SIGNIN_FLOW action can open a separate window, if need
 * be, and that if that window closes, the flow is considered aborted.
 */
add_task(async function test_fxa_signin_window_aborted() {
  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaWindowPromise = BrowserTestUtils.waitForNewWindow();
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
        data: {
          where: "window",
        },
      });
    });
    let fxaWindow = await fxaWindowPromise;
    Assert.ok(!fxaWindow.closed, "FxA window was not asked to close yet.");

    await BrowserTestUtils.closeWindow(fxaWindow);
    let result = await resultPromise;
    Assert.ok(!result, "FXA_SIGNIN_FLOW action's result should be false");
  });
});

/**
 * Tests that the FXA_SIGNIN_FLOW action can open a separate window, if need
 * be, and that if sign-in completes, that new window will close automatically.
 */
add_task(async function test_fxa_signin_window_success() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaWindowPromise = BrowserTestUtils.waitForNewWindow();
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
        data: {
          where: "window",
        },
      });
    });
    let fxaWindow = await fxaWindowPromise;
    Assert.ok(!fxaWindow.closed, "FxA window was not asked to close yet.");

    let windowClosed = BrowserTestUtils.windowClosed(fxaWindow);

    // We'll fake-out the UIState being in the STATUS_SIGNED_IN status
    // and not test the actual FxA sign-in mechanism.
    sandbox.stub(UIState, "get").returns({
      status: UIState.STATUS_SIGNED_IN,
      syncEnabled: true,
      email: "email@example.com",
    });

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    let result = await resultPromise;
    Assert.ok(result, "FXA_SIGNIN_FLOW action's result should be true");

    await windowClosed;
    Assert.ok(fxaWindow.closed, "Sign-in window was automatically closed.");
  });

  sandbox.restore();
});

/**
 * Tests that the FXA_SIGNIN_FLOW action can open a separate window, if need
 * be, and that if a new tab is opened in that window and the sign-in tab
 * is closed:
 *
 * 1. The new window isn't closed
 * 2. The sign-in is considered aborted.
 */
add_task(async function test_fxa_signin_window_multiple_tabs_aborted() {
  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaWindowPromise = BrowserTestUtils.waitForNewWindow();
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
        data: {
          where: "window",
        },
      });
    });
    let fxaWindow = await fxaWindowPromise;
    Assert.ok(!fxaWindow.closed, "FxA window was not asked to close yet.");
    let fxaTab = fxaWindow.gBrowser.selectedTab;
    await BrowserTestUtils.openNewForegroundTab(
      fxaWindow.gBrowser,
      "about:blank"
    );
    BrowserTestUtils.removeTab(fxaTab);

    let result = await resultPromise;
    Assert.ok(!result, "FXA_SIGNIN_FLOW action's result should be false");
    Assert.ok(!fxaWindow.closed, "FxA window was not asked to close.");
    await BrowserTestUtils.closeWindow(fxaWindow);
  });
});

/**
 * Tests that the FXA_SIGNIN_FLOW action can open a separate window, if need
 * be, and that if a new tab is opened in that window but then sign-in
 * completes
 *
 * 1. The new window isn't closed, but the sign-in tab is.
 * 2. The sign-in is considered a success.
 */
add_task(async function test_fxa_signin_window_multiple_tabs_success() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaWindowPromise = BrowserTestUtils.waitForNewWindow();
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
        data: {
          where: "window",
        },
      });
    });
    let fxaWindow = await fxaWindowPromise;
    Assert.ok(!fxaWindow.closed, "FxA window was not asked to close yet.");
    let fxaTab = fxaWindow.gBrowser.selectedTab;

    // This will open an about:blank tab in the background.
    await BrowserTestUtils.addTab(fxaWindow.gBrowser);
    let fxaTabClosed = BrowserTestUtils.waitForTabClosing(fxaTab);

    // We'll fake-out the UIState being in the STATUS_SIGNED_IN status
    // and not test the actual FxA sign-in mechanism.
    sandbox.stub(UIState, "get").returns({
      status: UIState.STATUS_SIGNED_IN,
      syncEnabled: true,
      email: "email@example.com",
    });

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    let result = await resultPromise;
    Assert.ok(result, "FXA_SIGNIN_FLOW action's result should be true");
    await fxaTabClosed;

    Assert.ok(!fxaWindow.closed, "FxA window was not asked to close.");
    await BrowserTestUtils.closeWindow(fxaWindow);
  });

  sandbox.restore();
});

/**
 * Tests that we can pass an entrypoint and UTM parameters to the FxA sign-in
 * page.
 */
add_task(async function test_fxa_signin_flow_entrypoint_utm_params() {
  await BrowserTestUtils.withNewTab("about:welcome", async browser => {
    let fxaTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let resultPromise = SpecialPowers.spawn(browser, [], async () => {
      return content.wrappedJSObject.AWSendToParent("SPECIAL_ACTION", {
        type: "FXA_SIGNIN_FLOW",
        data: {
          entrypoint: "test-entrypoint",
          extraParams: {
            utm_test1: "utm_test1",
            utm_test2: "utm_test2",
          },
        },
      });
    });
    let fxaTab = await fxaTabPromise;

    let uriParams = new URLSearchParams(fxaTab.linkedBrowser.currentURI.query);
    Assert.equal(uriParams.get("entrypoint"), "test-entrypoint");
    Assert.equal(uriParams.get("utm_test1"), "utm_test1");
    Assert.equal(uriParams.get("utm_test2"), "utm_test2");

    BrowserTestUtils.removeTab(fxaTab);
    await resultPromise;
  });
});
