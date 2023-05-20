"use strict";

const BASE_ORIGIN = "https://example.com";
const URI =
  BASE_ORIGIN +
  "/browser/browser/components/contextualidentity/test/browser/saveLink.sjs";

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

add_setup(async function () {
  info("Setting the prefs.");

  // make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.enabled", true],
      // This test does a redirect from https to http and it checks the
      // cookies. This is incompatible with the cookie SameSite schemeful
      // feature and we need to disable it.
      ["network.cookie.sameSite.schemeful", false],
      // This Test trys to download mixed content
      // so we need to make sure that this is not blocked
      ["dom.block_download_insecure", false],
    ],
  });
});

add_task(async function test() {
  info("Let's open a new window");
  let win = await BrowserTestUtils.openNewBrowserWindow();

  info("Opening tab with UCI: 1");
  let tab = BrowserTestUtils.addTab(win.gBrowser, URI + "?UCI=1", {
    userContextId: 1,
  });

  // select tab and make sure its browser is focused
  win.gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  info("Waiting to load content");
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    win.document,
    "popupshown"
  );

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#fff",
    { type: "contextmenu", button: 2 },
    win.gBrowser.selectedBrowser
  );
  info("Right clicked!");

  await popupShownPromise;
  info("Context menu opened");

  info("Let's create a temporary dir");
  let tempDir = createTemporarySaveDirectory();
  let destFile;

  MockFilePicker.displayDirectory = tempDir;
  MockFilePicker.showCallback = fp => {
    info("MockFilePicker showCallback");

    let fileName = fp.defaultString;
    destFile = tempDir.clone();
    destFile.append(fileName);

    MockFilePicker.setFiles([destFile]);
    MockFilePicker.filterIndex = 1; // kSaveAsType_URL

    info("MockFilePicker showCallback done");
  };

  let transferCompletePromise = new Promise(resolve => {
    function onTransferComplete(downloadSuccess) {
      ok(downloadSuccess, "File should have been downloaded successfully");
      resolve();
    }

    mockTransferCallback = onTransferComplete;
    mockTransferRegisterer.register();
  });

  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    tempDir.remove(true);
  });

  // Select "Save Link As" option from context menu
  let saveLinkCommand = win.document.getElementById("context-savelink");
  info("saveLinkCommand: " + saveLinkCommand);
  saveLinkCommand.doCommand();

  let contextMenu = win.document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
  info("popup hidden");

  await transferCompletePromise;

  // Let's query the SJS to know if the download happened with the correct cookie.
  let response = await fetch(URI + "?result=1");
  let text = await response.text();
  is(text, "Result:UCI=1", "Correct cookie used: -" + text + "-");

  info("Closing the window");
  await BrowserTestUtils.closeWindow(win);
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    info("create testsavedir!");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("return from createTempSaveDir: " + saveDir.path);
  return saveDir;
}
