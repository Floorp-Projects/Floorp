"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

// Using insecure HTTP URL for a test cases around HTTP/HTTPS download interaction
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const HTTP_LINK = `http://example.org/`;
const HTTPS_LINK = `https://example.org/`;
const TEST_PATH =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/dom/security/test/https-only/file_save_as.html";

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
const tempDir = createTemporarySaveDirectory();
MockFilePicker.displayDirectory = tempDir;

add_setup(async function () {
  info("Setting MockFilePicker.");
  mockTransferRegisterer.register();

  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    tempDir.remove(true);
  });
});

function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  saveDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return saveDir;
}

function createPromiseForObservingChannel(expectedUrl) {
  return new Promise(resolve => {
    let observer = (aSubject, aTopic) => {
      if (aTopic === "http-on-modify-request") {
        let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);

        if (httpChannel.URI.spec != expectedUrl) {
          return;
        }

        Services.obs.removeObserver(observer, "http-on-modify-request");
        resolve();
      }
    };

    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

function createPromiseForTransferComplete() {
  return new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      info("MockFilePicker showCallback");

      let fileName = fp.defaultString;
      let destFile = tempDir.clone();
      destFile.append(fileName);

      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete

      MockFilePicker.showCallback = null;
      mockTransferCallback = function (downloadSuccess) {
        ok(downloadSuccess, "File should have been downloaded successfully");
        mockTransferCallback = () => {};
        resolve();
      };
    };
  });
}

function createPromiseForConsoleError(message) {
  return new Promise((resolve, reject) => {
    function listener(msgObj) {
      let text = msgObj.message;
      if (text.includes(message)) {
        info(`Found occurence of '${message}'`);
        Services.console.unregisterListener(listener);
        resolve();
      }
    }
    Services.console.registerListener(listener);
  });
}

async function runTest(selector, expectedUrl, expectedError) {
  info(`Open a new tab for testing "Save link as" in context menu.`);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PATH);

  let popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown");

  let browser = gBrowser.selectedBrowser;

  info("Open the context menu.");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    {
      type: "contextmenu",
      button: 2,
    },
    browser
  );

  await popupShownPromise;

  let downloadEndPromise = expectedError
    ? createPromiseForConsoleError(expectedError)
    : createPromiseForTransferComplete();
  let observerPromise = createPromiseForObservingChannel(expectedUrl);

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );

  // Select "Save As" option from context menu.
  let saveElement = document.getElementById(`context-savelink`);
  info("Triggering the save process.");
  contextMenu.activateItem(saveElement);

  info("Waiting for the channel.");
  await observerPromise;

  info(
    expectedError
      ? "Waiting for error in console."
      : "Wait until the save is finished."
  );
  await downloadEndPromise;

  info("Wait until the menu is closed.");
  await popupHiddenPromise;

  BrowserTestUtils.removeTab(tab);
}

async function setHttpsFirstAndOnlyPrefs(httpsFirst, httpsOnly) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", httpsFirst],
      ["dom.security.https_only_mode", httpsOnly],
    ],
  });
}

add_task(async function testBaseline() {
  // Run with HTTPS-First and HTTPS-Only disabled
  await setHttpsFirstAndOnlyPrefs(false, false);
  await runTest("#insecure-link", HTTP_LINK, undefined);
  await runTest("#secure-link", HTTPS_LINK, undefined);
});

add_task(async function testHttpsFirst() {
  // Run with HTTPS-First enabled
  // The the user will get a warning about really wanting to download
  // from a insecure site, because we upgraded the top level document,
  // but the download is still insecure. In the future we also want to
  // upgrade these Save-As downloads.
  await setHttpsFirstAndOnlyPrefs(true, false);
  await runTest(
    "#insecure-link",
    HTTP_LINK,
    "Blocked downloading insecure content “http://example.org/”."
  );
  await runTest("#secure-link", HTTPS_LINK, undefined);
});

add_task(async function testHttpsOnly() {
  // Run with HTTPS-Only enabled
  // Should have same behaviour as HTTPS-First
  await setHttpsFirstAndOnlyPrefs(false, true);
  await runTest(
    "#insecure-link",
    HTTP_LINK,
    "Blocked downloading insecure content “http://example.org/”."
  );
  await runTest("#secure-link", HTTPS_LINK, undefined);
});
