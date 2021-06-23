const { HandlerServiceTestUtils } = ChromeUtils.import(
  "resource://testing-common/HandlerServiceTestUtils.jsm"
);

const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

async function selectPdfCategoryItem() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  info("Preferences page opened on the general pane.");

  await gBrowser.selectedBrowser.contentWindow.promiseLoadHandlersList;
  info("Apps list loaded.");

  let win = gBrowser.selectedBrowser.contentWindow;
  let container = win.document.getElementById("handlersView");
  let pdfCategory = container.querySelector(
    "richlistitem[type='application/pdf']"
  );

  pdfCategory.closest("richlistbox").selectItem(pdfCategory);
  Assert.ok(pdfCategory.selected, "Should be able to select our item.");

  return pdfCategory;
}

async function selectItemInPopup(item, list) {
  let popup = list.menupopup;
  let popupShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
  // Synthesizing the mouse on the .actionsMenu menulist somehow just selects
  // the top row. Probably something to do with the multiple layers of anon
  // content - workaround by using the `.open` setter instead.
  list.open = true;
  await popupShown;
  let popupHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");

  item.click();
  popup.hidePopup();
  await popupHidden;
  return item;
}

add_task(async function alwaysAskPreferenceWorks() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", true],
    ],
  });

  let pdfCategory = await selectPdfCategoryItem();
  let list = pdfCategory.querySelector(".actionsMenu");

  let alwaysAskItem = list.querySelector(
    `menuitem[action='${Ci.nsIHandlerInfo.alwaysAsk}']`
  );

  await selectItemInPopup(alwaysAskItem, list);
  Assert.equal(
    list.selectedItem,
    alwaysAskItem,
    "Should have selected 'always ask' for pdf"
  );
  let alwaysAskBeforeHandling = HandlerServiceTestUtils.getHandlerInfo(
    pdfCategory.getAttribute("type")
  ).alwaysAskBeforeHandling;
  Assert.ok(
    alwaysAskBeforeHandling,
    "Should have turned on 'always asking before handling'"
  );

  let domWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "empty_pdf_file.pdf"
  );

  let domWindow = await domWindowPromise;
  let dialog = domWindow.document.querySelector("#unknownContentType");
  let button = dialog.getButton("cancel");

  await TestUtils.waitForCondition(
    () => !button.disabled,
    "Wait for Cancel button to get enabled"
  );
  Assert.ok(dialog, "Dialog should be shown");
  dialog.cancelDialog();
  BrowserTestUtils.removeTab(loadingTab);

  gBrowser.removeCurrentTab();
});

add_task(async function handleInternallyPreferenceWorks() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", true],
    ],
  });

  let pdfCategory = await selectPdfCategoryItem();
  let list = pdfCategory.querySelector(".actionsMenu");

  let handleInternallyItem = list.querySelector(
    `menuitem[action='${Ci.nsIHandlerInfo.handleInternally}']`
  );

  await selectItemInPopup(handleInternallyItem, list);
  Assert.equal(
    list.selectedItem,
    handleInternallyItem,
    "Should have selected 'handle internally' for pdf"
  );

  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "empty_pdf_file.pdf"
  );

  await ContentTask.spawn(loadingTab.linkedBrowser, null, async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.readyState == "complete"
    );
    Assert.ok(
      content.document.querySelector("div#viewer"),
      "document content has viewer UI"
    );
  });

  BrowserTestUtils.removeTab(loadingTab);

  gBrowser.removeCurrentTab();
});

add_task(async function saveToDiskPreferenceWorks() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", true],
    ],
  });

  let pdfCategory = await selectPdfCategoryItem();
  let list = pdfCategory.querySelector(".actionsMenu");

  let saveToDiskItem = list.querySelector(
    `menuitem[action='${Ci.nsIHandlerInfo.saveToDisk}']`
  );

  await selectItemInPopup(saveToDiskItem, list);
  Assert.equal(
    list.selectedItem,
    saveToDiskItem,
    "Should have selected 'save to disk' for pdf"
  );

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });

  let downloadFinishedPromise = new Promise(resolve => {
    publicList.addView({
      onDownloadChanged(download) {
        if (download.succeeded || download.error) {
          publicList.removeView(this);
          resolve(download);
        }
      },
    });
  });

  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "empty_pdf_file.pdf"
  );

  let download = await downloadFinishedPromise;
  BrowserTestUtils.removeTab(loadingTab);

  Assert.ok(
    await OS.File.exists(download.target.path),
    "The file should have been downloaded."
  );

  try {
    info("removing " + download.target.path);
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(download.target.path, 0o600);
    }
    await IOUtils.remove(download.target.path);
  } catch (ex) {
    info("The file " + download.target.path + " is not removed, " + ex);
  }

  gBrowser.removeCurrentTab();
});

add_task(async function useSystemDefaultPreferenceWorks() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", true],
    ],
  });

  let pdfCategory = await selectPdfCategoryItem();
  let list = pdfCategory.querySelector(".actionsMenu");

  let useSystemDefaultItem = list.querySelector(
    `menuitem[action='${Ci.nsIHandlerInfo.useSystemDefault}']`
  );

  await selectItemInPopup(useSystemDefaultItem, list);
  Assert.equal(
    list.selectedItem,
    useSystemDefaultItem,
    "Should have selected 'use system default' for pdf"
  );

  let oldLaunchFile = DownloadIntegration.launchFile;

  let waitForLaunchFileCalled = new Promise(resolve => {
    DownloadIntegration.launchFile = () => {
      ok(true, "The file should be launched with an external application");
      resolve();
    };
  });

  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "empty_pdf_file.pdf"
  );

  info("Waiting until DownloadIntegration.launchFile is called");
  await waitForLaunchFileCalled;

  DownloadIntegration.launchFile = oldLaunchFile;

  BrowserTestUtils.removeTab(loadingTab);

  gBrowser.removeCurrentTab();
});
