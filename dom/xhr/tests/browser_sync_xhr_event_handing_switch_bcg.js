const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const childURL = `${baseURL}empty.html`;
const parentURL = `${baseURL}empty_parent.html`;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.input_events.canSuspendInBCG.enabled", true]],
  });
  if (!Services.appinfo.fissionAutostart) {
    // Make sure the tab that is opened with noopener
    // also in the same process as the parent.
    await SpecialPowers.pushPrefEnv({
      set: [["dom.ipc.processCount", 1]],
    });
  }
});

async function checkInputManagerStatus(openChildInSameBCG) {
  let childTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    childURL,
    true,
    true
  );

  let xhrTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    parentURL,
    true
  );

  let xhrTabIsHidden = BrowserTestUtils.waitForContentEvent(
    xhrTab.linkedBrowser,
    "visibilitychange"
  );

  await SpecialPowers.spawn(
    xhrTab.linkedBrowser.browsingContext,
    [openChildInSameBCG, childURL],
    async function (sameBCG, url) {
      if (sameBCG) {
        content.open(url);
      } else {
        content.open(url, "", "noopener");
      }
    }
  );

  await childTabPromise;
  await xhrTabIsHidden;

  let xhrRequestIsReady = BrowserTestUtils.waitForContentEvent(
    xhrTab.linkedBrowser,
    "xhrRequestIsReady"
  );

  let xhrRequest = SpecialPowers.spawn(
    xhrTab.linkedBrowser.browsingContext,
    [],
    () => {
      var xhr = new content.XMLHttpRequest();
      xhr.open("GET", "slow.sjs", false);
      content.document.dispatchEvent(
        new content.Event("xhrRequestIsReady", { bubbles: true })
      );
      xhr.send(null);
    }
  );

  // Need to wait for the xhrIsReady event because spawn is async,
  // so the content needs to give us a signal that the sync XHR request
  // has started
  await xhrRequestIsReady;

  let childTab = gBrowser.tabs[2];

  // Since the xhrTab has started the sync XHR request,
  // the InputTaskManager should be suspended here
  // if it is in the same browsing context as the opener
  let isSuspendedBeforeSwitch = await SpecialPowers.spawn(
    childTab.linkedBrowser.browsingContext,
    [],
    () => {
      var utils = SpecialPowers.getDOMWindowUtils(content);
      return utils.isInputTaskManagerSuspended;
    }
  );

  is(
    isSuspendedBeforeSwitch,
    openChildInSameBCG,
    "InputTaskManager should be suspended before tab switching"
  );

  // Switching away from the childTab and switching back to
  // test the status of InputTaskManager gets updated accordingly
  // based on whether the childTab is in the same BCG as the xhrTab or not.
  await BrowserTestUtils.switchTab(gBrowser, xhrTab);
  await BrowserTestUtils.switchTab(gBrowser, childTab);

  let isSuspendedAfterTabSwitch = await SpecialPowers.spawn(
    childTab.linkedBrowser.browsingContext,
    [],
    () => {
      var utils = SpecialPowers.getDOMWindowUtils(content);
      return utils.isInputTaskManagerSuspended;
    }
  );

  is(
    isSuspendedAfterTabSwitch,
    openChildInSameBCG,
    "InputTaskManager should be either suspended or not suspended based whether childTab was opened in the same BCG"
  );

  await xhrRequest;

  let isSuspendedAfterXHRRequest = await SpecialPowers.spawn(
    xhrTab.linkedBrowser.browsingContext,
    [],
    () => {
      var utils = SpecialPowers.getDOMWindowUtils(content);
      return utils.isInputTaskManagerSuspended;
    }
  );

  is(
    isSuspendedAfterXHRRequest,
    false,
    "InputTaskManager should not be suspended before after the sync XHR is done"
  );

  gBrowser.removeTab(xhrTab);
  gBrowser.removeTab(childTab);
}

add_task(async function switchBCG() {
  await checkInputManagerStatus(true);
  await checkInputManagerStatus(false);
});
