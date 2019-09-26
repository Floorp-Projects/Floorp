"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderExtension: "resource:///modules/UrlbarProviderExtension.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

async function loadExtension(options = {}) {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
    },
    isPrivileged: true,
    background() {
      browser.test.onMessage.addListener(options => {
        browser.urlbar.onBehaviorRequested.addListener(query => {
          return "restricting";
        }, "test");
        browser.urlbar.onResultsRequested.addListener(query => {
          return [
            {
              type: "tip",
              source: "local",
              heuristic: true,
              payload: {
                text: "Test",
                buttonText: "OK",
                data: "testData",
                buttonUrl: options.buttonUrl,
                helpUrl: options.helpUrl,
              },
            },
          ];
        }, "test");
        browser.urlbar.onResultPicked.addListener((payload, details) => {
          browser.test.assertEq(payload.text, "Test", "payload.text");
          browser.test.assertEq(payload.buttonText, "OK", "payload.buttonText");
          browser.test.assertEq(payload.data, "testData", "payload.data");
          browser.test.sendMessage("onResultPicked received", details);
        }, "test");
      });
    },
  });
  await ext.startup();
  ext.sendMessage(options);

  // Wait for the provider to be registered before continuing.  The provider
  // will be registered once the parent process receives the first addListener
  // call from the extension.  There's no better way to do this, unfortunately.
  // For example, if the extension sends a message to the test after it adds its
  // listeners and then we wait here for that message, there's no guarantee that
  // the addListener calls will have been received in the parent yet.
  await BrowserTestUtils.waitForCondition(
    () => UrlbarProvidersManager.getProvider("test"),
    "Waiting for provider to be registered"
  );

  Assert.ok(
    UrlbarProvidersManager.getProvider("test"),
    "Provider should have been registered"
  );
  return ext;
}

add_task(async function setUp() {
  // Set the notification timeout to a really high value to avoid intermittent
  // failures due to the mock extensions not responding in time.
  let originalTimeout = UrlbarProviderExtension.notificationTimeout;
  UrlbarProviderExtension.notificationTimeout = 5000;
  registerCleanupFunction(() => {
    UrlbarProviderExtension.notificationTimeout = originalTimeout;
  });
});

// Loads an extension without a main button URL and presses enter on the main
// button.
add_task(async function testOnResultPicked_mainButton_noURL_enter() {
  let ext = await loadExtension();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
  });
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  let details = await ext.awaitMessage("onResultPicked received");
  Assert.deepEqual(details, { helpPicked: false });
  await ext.unload();
});

// Loads an extension without a main button URL and clicks the main button.
add_task(async function testOnResultPicked_mainButton_noURL_mouse() {
  let ext = await loadExtension();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
  });
  let mainButton = document.querySelector(
    "#urlbarView-row-0 .urlbarView-tip-button"
  );
  Assert.ok(mainButton);
  EventUtils.synthesizeMouseAtCenter(mainButton, {});
  let details = await ext.awaitMessage("onResultPicked received");
  Assert.deepEqual(details, { helpPicked: false });
  await ext.unload();
});

// Loads an extension with a main button URL and presses enter on the main
// button.
add_task(async function testOnResultPicked_mainButton_url_enter() {
  let ext = await loadExtension({ buttonUrl: "http://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_Enter");
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "http://example.com/");
  });
  await ext.unload();
});

// Loads an extension with a main button URL and clicks the main button.
add_task(async function testOnResultPicked_mainButton_url_mouse() {
  let ext = await loadExtension({ buttonUrl: "http://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    let mainButton = document.querySelector(
      "#urlbarView-row-0 .urlbarView-tip-button"
    );
    Assert.ok(mainButton);
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    EventUtils.synthesizeMouseAtCenter(mainButton, {});
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "http://example.com/");
  });
  await ext.unload();
});

// Loads an extension without a help button URL and presses enter on the help
// button.
add_task(async function testOnResultPicked_helpButton_noURL_enter() {
  let ext = await loadExtension();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
  });
  EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
  EventUtils.synthesizeKey("KEY_Enter");
  let details = await ext.awaitMessage("onResultPicked received");
  Assert.deepEqual(details, { helpPicked: true });
  await ext.unload();
});

// Loads an extension without a help button URL and clicks the help button.
add_task(async function testOnResultPicked_helpButton_noURL_mouse() {
  let ext = await loadExtension();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
  });
  let helpButton = document.querySelector(
    "#urlbarView-row-0 .urlbarView-tip-help"
  );
  Assert.ok(helpButton);
  EventUtils.synthesizeMouseAtCenter(helpButton, {});
  let details = await ext.awaitMessage("onResultPicked received");
  Assert.deepEqual(details, { helpPicked: true });
  await ext.unload();
});

// Loads an extension with a help button URL and presses enter on the help
// button.
add_task(async function testOnResultPicked_helpButton_url_enter() {
  let ext = await loadExtension({ helpUrl: "http://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
    EventUtils.synthesizeKey("KEY_Enter");
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "http://example.com/");
  });
  await ext.unload();
});

// Loads an extension with a help button URL and clicks the help button.
add_task(async function testOnResultPicked_helpButton_url_mouse() {
  let ext = await loadExtension({ helpUrl: "http://example.com/" });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "test",
    });
    let helpButton = document.querySelector(
      "#urlbarView-row-0 .urlbarView-tip-help"
    );
    Assert.ok(helpButton);
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    ext.onMessage("onResultPicked received", () => {
      Assert.ok(false, "onResultPicked should not be called");
    });
    EventUtils.synthesizeMouseAtCenter(helpButton, {});
    await loadedPromise;
    Assert.equal(gBrowser.currentURI.spec, "http://example.com/");
  });
  await ext.unload();
});
