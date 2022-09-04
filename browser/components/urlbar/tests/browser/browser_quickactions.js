/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests QuickActions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
});

const DUMMY_PAGE =
  "http://example.com/browser/browser/base/content/test/general/dummy_page.html";

let testActionCalled = 0;

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.shortcuts.quickactions", true],
      ["screenshots.browser.component.enabled", true],
      ["extensions.screenshots.disabled", false],
    ],
  });

  UrlbarProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
    label: "quickactions-downloads",
    onPick: () => testActionCalled++,
  });

  registerCleanupFunction(() => {
    UrlbarProviderQuickActions.removeAction("testaction");
  });
});

add_task(async function basic() {
  info("The action isnt shown when not matched");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "nomatch",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "We did no match anything"
  );

  info("A prefix of the command matches");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testact",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );

  info("The callback of the action is fired when selected");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  Assert.equal(testActionCalled, 1, "Test actionwas called");
});

add_task(async function test_label_command() {
  info("A prefix of the label matches");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "Open Dow",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );

  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
  Assert.equal(result.providerName, "quickactions");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
});

add_task(async function enter_search_mode_button() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  let oneOffButton = await TestUtils.waitForCondition(() =>
    window.document.getElementById("urlbar-engine-one-off-item-actions")
  );
  Assert.ok(oneOffButton, "One off button is available when preffed on");
  EventUtils.synthesizeMouseAtCenter(oneOffButton, {}, window);

  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    entry: "oneoff",
  });

  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  Assert.ok(true, "Actions are shown when we enter actions search mode.");

  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
});

add_task(async function enter_search_mode_key() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "> ",
  });
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    entry: "typed",
  });
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
});

add_task(async function test_disabled() {
  testActionCalled = 0;
  UrlbarProviderQuickActions.addAction("disabledaction", {
    commands: ["disabledaction"],
    isActive: () => false,
    label: "quickactions-restart",
    onPick: () => testActionCalled++,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "disabled",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "The action is displayed"
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-button-help"
    ),
    "The selected element should be the onboarding button."
  );

  let disabledButton = window.document.querySelector(
    ".urlbarView-row[dynamicType=quickactions]"
  );
  EventUtils.synthesizeMouseAtCenter(disabledButton, {});
  Assert.equal(
    testActionCalled,
    0,
    "onPick for disabled action was not called"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
  UrlbarProviderQuickActions.removeAction("disabledaction");
});

add_task(async function test_screenshot_disabled() {
  let onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "about:blank"
  );
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:blank");
  await onLoaded;

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "screenshot",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "The action is displayed"
  );
  let screenshotButton = window.document.querySelector(
    ".urlbarView-row[dynamicType=quickactions] .urlbarView-quickaction-row"
  );
  Assert.equal(
    screenshotButton.getAttribute("disabled"),
    "disabled",
    "Screenshot button is disabled on about pages"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
});

add_task(async function match_in_phrase() {
  UrlbarProviderQuickActions.addAction("newtestaction", {
    commands: ["matchingstring"],
    label: "quickactions-downloads",
  });

  info("The action is matched when at end of input");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "Test we match at end of matchingstring",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
  UrlbarProviderQuickActions.removeAction("newtestaction");
});

async function isScreenshotInitialized() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let screenshotsChild = content.windowGlobalChild.getActor(
      "ScreenshotsComponent"
    );
    return screenshotsChild?._overlay?._initialized;
  });
}

add_task(async function test_screenshot() {
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, DUMMY_PAGE);
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    DUMMY_PAGE
  );

  info("The action is matched when at end of input");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "screenshot",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
  Assert.equal(result.providerName, "quickactions");

  info("Trigger the screenshot mode");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  await TestUtils.waitForCondition(
    isScreenshotInitialized,
    "Screenshot component is active"
  );

  info("Press Escape to exit screenshot mode");
  EventUtils.synthesizeKey("KEY_Escape", {}, window);
  await TestUtils.waitForCondition(
    async () => !(await isScreenshotInitialized()),
    "Screenshot component has been dismissed"
  );

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
});

add_task(async function test_other_search_mode() {
  let defaultEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );
  defaultEngine.alias = "testalias";
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(defaultEngine);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: defaultEngine.alias + " ",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    0,
    "The results should be empty as no actions are displayed in other search modes"
  );
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: defaultEngine.name,
    entry: "typed",
  });
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
  Services.search.setDefault(oldDefaultEngine);
});

let COMMANDS_TESTS = [
  ["add-ons", async () => isSelected("button[name=discover]")],
  ["plugins", async () => isSelected("button[name=plugin]")],
  ["extensions", async () => isSelected("button[name=extension]")],
  ["themes", async () => isSelected("button[name=theme]")],
];

let isSelected = async selector =>
  SpecialPowers.spawn(gBrowser.selectedBrowser, [selector], arg => {
    return ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(arg)?.hasAttribute("selected")
    );
  });

add_task(async function test_pages() {
  for (const [cmd, testFun] of COMMANDS_TESTS) {
    info(`Testing ${cmd} command is triggered`);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: cmd,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    Assert.ok(
      await testFun(),
      `The command "${cmd}" passed completed its test`
    );
    await BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_about_pages_refocused() {
  const testData = [
    {
      input: "downloads",
      uri: "about:downloads",
    },
    {
      input: "logins",
      uri: "about:logins",
    },
    {
      input: "settings",
      uri: "about:preferences",
    },
  ];

  for (const { input, uri } of testData) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: input,
    });

    info("Open about page by quick action");
    let originalTab = gBrowser.selectedTab;
    let onLoad = BrowserTestUtils.waitForNewTab(gBrowser, uri);
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
    let newTab = await onLoad;

    info("Do the same quick action in original tab");
    gBrowser.selectedTab = originalTab;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: input,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
    Assert.equal(
      gBrowser.selectedTab,
      newTab,
      "Switched to the tab that is opening the about page"
    );
    Assert.equal(gBrowser.tabs.length, 2, "Not opened a new tab");

    BrowserTestUtils.removeTab(newTab);
  }
});

const assertActionButtonStatus = async (name, expectedEnabled, description) => {
  await BrowserTestUtils.waitForCondition(() =>
    window.document.querySelector(`[data-key=${name}]`)
  );
  const target = window.document.querySelector(`[data-key=${name}]`);
  Assert.equal(!target.hasAttribute("disabled"), expectedEnabled, description);
};

add_task(async function test_inspector() {
  const testData = [
    {
      description: "Test for 'about:' page",
      page: "about:home",
      isDevToolsUser: true,
      actionVisible: true,
      actionEnabled: true,
    },
    {
      description: "Test for another 'about:' page",
      page: "about:about",
      isDevToolsUser: true,
      actionVisible: true,
      actionEnabled: true,
    },
    {
      description: "Test for another devtools-toolbox page",
      page: "about:devtools-toolbox",
      isDevToolsUser: true,
      actionVisible: true,
      actionEnabled: false,
    },
    {
      description: "Test for web content",
      page: "http://example.com",
      isDevToolsUser: true,
      actionVisible: true,
      actionEnabled: true,
    },
    {
      description: "Test for disabled DevTools",
      page: "http://example.com",
      prefs: [["devtools.policy.disabled", true]],
      isDevToolsUser: true,
      actionVisible: true,
      actionEnabled: false,
    },
    {
      description: "Test for not DevTools user",
      page: "http://example.com",
      isDevToolsUser: false,
      actionVisible: true,
      actionEnabled: false,
    },
    {
      description: "Test for fully disabled",
      page: "http://example.com",
      prefs: [["devtools.policy.disabled", true]],
      isDevToolsUser: false,
      actionVisible: false,
    },
  ];

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (const {
    description,
    page,
    prefs = [],
    isDevToolsUser,
    actionEnabled,
    actionVisible,
  } of testData) {
    info(description);

    info("Set preferences");
    await SpecialPowers.pushPrefEnv({
      set: [...prefs, ["devtools.selfxss.count", isDevToolsUser ? 5 : 0]],
    });

    info("Check the button status");
    const onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, page);
    await onLoad;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "inspector",
    });

    if (actionVisible) {
      await assertActionButtonStatus(
        "inspect",
        actionEnabled,
        "The status of action button is correct"
      );
    } else {
      const target = window.document.querySelector(`[data-key=inspect]`);
      Assert.ok(!target, "Inspect button should not be displayed");
    }

    await SpecialPowers.popPrefEnv();

    if (!actionEnabled) {
      continue;
    }

    info("Do inspect action");
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
    await BrowserTestUtils.waitForCondition(
      () => DevToolsShim.hasToolboxForTab(gBrowser.selectedTab),
      "Wait for opening inspector for current selected tab"
    );
    const toolbox = await DevToolsShim.getToolboxForTab(gBrowser.selectedTab);
    await BrowserTestUtils.waitForCondition(
      () => toolbox.currentToolId === "inspector",
      "Wait until the inspector is selected"
    );

    info("Do inspect action again in the same page during opening inspector");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "inspector",
    });
    await assertActionButtonStatus(
      "inspect",
      false,
      "The action button should be disabled since the inspector is already opening"
    );

    info(
      "Select another tool to check whether the inspector will be selected in next test even if the previous tool is not inspector"
    );
    await toolbox.selectTool("options");
    await toolbox.destroy();
  }

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_viewsource() {
  info("Check the button status of when the page is not web content");
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:home",
    waitForLoad: true,
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "viewsource",
  });
  await assertActionButtonStatus(
    "viewsource",
    true,
    "Should be enabled even if the page is not web content"
  );

  info("Check the button status of when the page is web content");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "http://example.com");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "viewsource",
  });
  await assertActionButtonStatus(
    "viewsource",
    true,
    "Should be enabled on web content as well"
  );

  info("Do view source action");
  const onLoad = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "view-source:http://example.com/"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  const viewSourceTab = await onLoad;

  info("Do view source action on the view-source page");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "viewsource",
  });
  await assertActionButtonStatus(
    "viewsource",
    false,
    "Should be disabled since the page is view-source content"
  );

  // Clean up.
  BrowserTestUtils.removeTab(viewSourceTab);
  BrowserTestUtils.removeTab(tab);
});

async function doAlertDialogTest({ input, dialogContentURI }) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: input,
  });

  const onDialog = BrowserTestUtils.promiseAlertDialog(null, null, {
    isSubDialog: true,
    callback: win => {
      Assert.equal(win.location.href, dialogContentURI, "The dialog is opened");
      EventUtils.synthesizeKey("KEY_Escape", {}, win);
    },
  });

  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);

  await onDialog;
}

add_task(async function test_refresh() {
  await doAlertDialogTest({
    input: "refresh",
    dialogContentURI: "chrome://global/content/resetProfile.xhtml",
  });
});

add_task(async function test_clear() {
  await doAlertDialogTest({
    input: "clear",
    dialogContentURI: "chrome://browser/content/sanitize.xhtml",
  });
});

async function doUpdateActionTest(isActiveExpected, description) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "update",
  });

  await assertActionButtonStatus("update", isActiveExpected, description);
}

add_task(async function test_update() {
  if (!AppConstants.MOZ_UPDATER) {
    await doUpdateActionTest(
      false,
      "Should be disabled since not AppConstants.MOZ_UPDATER"
    );
    return;
  }

  const sandbox = sinon.createSandbox();
  try {
    sandbox.stub(AppUpdater.prototype, "isReadyForRestart").get(() => false);
    await doUpdateActionTest(
      false,
      "Should be disabled since AppUpdater.isReadyForRestart returns false"
    );
    sandbox.stub(AppUpdater.prototype, "isReadyForRestart").get(() => true);
    await doUpdateActionTest(
      true,
      "Should be enabled since AppUpdater.isReadyForRestart returns true"
    );
  } finally {
    sandbox.restore();
  }
});
