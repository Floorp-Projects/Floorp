/**
 * Bug 1508355 - A test case for ensuring the saving channel has a correct first
 *               party domain when going through different "Save ... AS."
 */

"use strict";

/* import-globals-from ../../../../../toolkit/content/tests/browser/common/mockTransfer.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

const TEST_FIRST_PARTY = "example.com";
const TEST_ORIGIN = `http://${TEST_FIRST_PARTY}`;
const TEST_BASE_PATH =
  "/browser/browser/components/originattributes/test/browser/";
const TEST_PATH = `${TEST_BASE_PATH}file_saveAs.sjs`;
const TEST_PATH_VIDEO = `${TEST_BASE_PATH}file_thirdPartyChild.video.ogv`;
const TEST_PATH_IMAGE = `${TEST_BASE_PATH}file_favicon.png`;

// For the "Save Page As" test, we will check the channel of the sub-resource
// within the page. In this case, it is a image.
const TEST_PATH_PAGE = `${TEST_BASE_PATH}file_favicon.png`;

// For the "Save Frame As" test, we will check the channel of the sub-resource
// within the frame. In this case, it is a image.
const TEST_PATH_FRAME = `${TEST_BASE_PATH}file_favicon.png`;

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
const tempDir = createTemporarySaveDirectory();
MockFilePicker.displayDirectory = tempDir;

add_task(async function setup() {
  info("Setting the prefs.");

  await SpecialPowers.pushPrefEnv({
    set: [["privacy.firstparty.isolate", true]],
  });

  info("Setting MockFilePicker.");
  mockTransferRegisterer.register();

  registerCleanupFunction(function() {
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

function createPromiseForObservingChannel(aURL, aFirstParty) {
  return new Promise(resolve => {
    let observer = (aSubject, aTopic) => {
      if (aTopic === "http-on-modify-request") {
        let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
        let reqLoadInfo = httpChannel.loadInfo;

        // Make sure this is the request which we want to check.
        if (!httpChannel.URI.spec.endsWith(aURL)) {
          return;
        }

        info(`Checking loadInfo for URI: ${httpChannel.URI.spec}\n`);
        is(
          reqLoadInfo.originAttributes.firstPartyDomain,
          aFirstParty,
          "The loadInfo has correct first party domain"
        );

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
      mockTransferCallback = function(downloadSuccess) {
        ok(downloadSuccess, "File should have been downloaded successfully");
        mockTransferCallback = () => {};
        resolve();
      };
    };
  });
}

async function doCommandForFrameType() {
  info("Opening the frame sub-menu under the context menu.");
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let frameMenu = contextMenu.querySelector("#frame");
  let frameMenuPopup = frameMenu.menupopup;
  let frameMenuPopupPromise = BrowserTestUtils.waitForEvent(
    frameMenuPopup,
    "popupshown"
  );

  frameMenu.openMenu(true);
  await frameMenuPopupPromise;

  info("Triggering the save process.");
  let saveFrameCommand = contextMenu.querySelector("#context-saveframe");
  frameMenuPopup.activateItem(saveFrameCommand);
}

add_task(async function test_setup() {
  // Make sure SearchService is ready for it to be called.
  await Services.search.init();
});

add_task(async function testContextMenuSaveAs() {
  const TEST_DATA = [
    { type: "link", path: TEST_PATH, target: "#link1" },
    { type: "video", path: TEST_PATH_VIDEO, target: "#video1" },
    { type: "image", path: TEST_PATH_IMAGE, target: "#image1" },
    { type: "page", path: TEST_PATH_PAGE, target: "body" },
    {
      type: "frame",
      path: TEST_PATH_FRAME,
      target: "body",
      doCommandFunc: doCommandForFrameType,
    },
  ];

  for (const data of TEST_DATA) {
    info(`Open a new tab for testing "Save ${data.type} as" in context menu.`);
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      `${TEST_ORIGIN}${TEST_PATH}?${data.type}=1`
    );

    let popupShownPromise = BrowserTestUtils.waitForEvent(
      document,
      "popupshown"
    );

    let browser = gBrowser.selectedBrowser;

    if (data.type === "frame") {
      browser = await SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        () => content.document.getElementById("frame1").browsingContext
      );
    }

    info("Open the context menu.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      data.target,
      {
        type: "contextmenu",
        button: 2,
      },
      browser
    );

    await popupShownPromise;

    let transferCompletePromise = createPromiseForTransferComplete();
    let observerPromise = createPromiseForObservingChannel(
      data.path,
      TEST_FIRST_PARTY
    );

    let contextMenu = document.getElementById("contentAreaContextMenu");
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popuphidden"
    );

    // Select "Save As" option from context menu.
    if (!data.doCommandFunc) {
      let saveElement = document.getElementById(`context-save${data.type}`);
      info("Triggering the save process.");
      contextMenu.activateItem(saveElement);
    } else {
      await data.doCommandFunc();
    }

    info("Waiting for the channel.");
    await observerPromise;

    info("Wait until the save is finished.");
    await transferCompletePromise;

    info("Wait until the menu is closed.");
    await popupHiddenPromise;

    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function testFileMenuSavePageAs() {
  info(`Open a new tab for testing "Save Page AS" in the file menu.`);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `${TEST_ORIGIN}${TEST_PATH}?page=1`
  );

  let transferCompletePromise = createPromiseForTransferComplete();
  let observerPromise = createPromiseForObservingChannel(
    TEST_PATH_PAGE,
    TEST_FIRST_PARTY
  );

  let menubar = document.getElementById("main-menubar");
  let filePopup = document.getElementById("menu_FilePopup");

  // We only use the shortcut keys to open the file menu in Windows and Linux.
  // Mac doesn't have a shortcut to only open the file menu. Instead, we directly
  // trigger the save in MAC without any UI interactions.
  if (Services.appinfo.OS !== "Darwin") {
    let menubarActive = BrowserTestUtils.waitForEvent(
      menubar,
      "DOMMenuBarActive"
    );
    EventUtils.synthesizeKey("KEY_F10");
    await menubarActive;

    let popupShownPromise = BrowserTestUtils.waitForEvent(
      filePopup,
      "popupshown"
    );
    // In window, it still needs one extra down key to open the file menu.
    if (Services.appinfo.OS === "WINNT") {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    await popupShownPromise;
  }

  info("Triggering the save process.");
  let fileSavePageAsElement = document.getElementById("menu_savePage");
  fileSavePageAsElement.doCommand();

  info("Waiting for the channel.");
  await observerPromise;

  info("Wait until the save is finished.");
  await transferCompletePromise;

  // Close the file menu.
  if (Services.appinfo.OS !== "Darwin") {
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      filePopup,
      "popuphidden"
    );
    filePopup.hidePopup();
    await popupHiddenPromise;
  }

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testPageInfoMediaSaveAs() {
  info(
    `Open a new tab for testing "Save AS" in the media panel of the page info.`
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `${TEST_ORIGIN}${TEST_PATH}?pageinfo=1`
  );

  info("Open the media panel of the pageinfo.");
  let pageInfo = BrowserPageInfo(
    gBrowser.selectedBrowser.currentURI.spec,
    "mediaTab"
  );

  await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");

  let imageTree = pageInfo.document.getElementById("imagetree");
  let imageRowsNum = imageTree.view.rowCount;

  is(imageRowsNum, 2, "There should be two media items here.");

  for (let i = 0; i < imageRowsNum; i++) {
    imageTree.view.selection.select(i);
    imageTree.ensureRowIsVisible(i);
    imageTree.focus();

    let url = pageInfo.gImageView.data[i][0]; // COL_IMAGE_ADDRESS
    info(`Start to save the media item with URL: ${url}`);

    let transferCompletePromise = createPromiseForTransferComplete();
    let observerPromise = createPromiseForObservingChannel(
      url,
      TEST_FIRST_PARTY
    );

    info("Triggering the save process.");
    let saveElement = pageInfo.document.getElementById("imagesaveasbutton");
    saveElement.doCommand();

    info("Waiting for the channel.");
    await observerPromise;

    info("Wait until the save is finished.");
    await transferCompletePromise;
  }

  pageInfo.close();
  BrowserTestUtils.removeTab(tab);
});
