/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const DATA_PDF = atob(
  "JVBERi0xLjANCjEgMCBvYmo8PC9UeXBlL0NhdGFsb2cvUGFnZXMgMiAwIFI+PmVuZG9iaiAyIDAgb2JqPDwvVHlwZS9QYWdlcy9LaWRzWzMgMCBSXS9Db3VudCAxPj5lbmRvYmogMyAwIG9iajw8L1R5cGUvUGFnZS9NZWRpYUJveFswIDAgMyAzXT4+ZW5kb2JqDQp4cmVmDQowIDQNCjAwMDAwMDAwMDAgNjU1MzUgZg0KMDAwMDAwMDAxMCAwMDAwMCBuDQowMDAwMDAwMDUzIDAwMDAwIG4NCjAwMDAwMDAxMDIgMDAwMDAgbg0KdHJhaWxlcjw8L1NpemUgNC9Sb290IDEgMCBSPj4NCnN0YXJ0eHJlZg0KMTQ5DQolRU9G"
);
let gDownloadDir;

/*
  Coverage for opening downloaded PDFs from download views
*/

const TestCases = [
  {
    name: "Download panel, default click behavior",
    whichUI: "downloadPanel",
    itemSelector: "#downloadsListBox richlistitem .downloadMainArea",
    async userEvents(itemTarget, win) {
      EventUtils.synthesizeMouseAtCenter(itemTarget, {}, win);
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
  {
    name: "Download panel, open from keyboard",
    whichUI: "downloadPanel",
    itemSelector: "#downloadsListBox richlistitem .downloadMainArea",
    async userEvents(itemTarget, win) {
      itemTarget.focus();
      EventUtils.synthesizeKey("VK_RETURN", {}, win);
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
  {
    name: "Library all downloads dialog, default click behavior",
    whichUI: "allDownloads",
    async userEvents(itemTarget, win) {
      // double click
      await triggerDblclickOn(itemTarget, {}, win);
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
  {
    name: "Library all downloads dialog, open tab from keyboard",
    whichUI: "allDownloads",
    async userEvents(itemTarget, win) {
      itemTarget.focus();
      EventUtils.synthesizeKey("VK_RETURN", {}, win);
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
  {
    name: "about:downloads, default click behavior",
    whichUI: "aboutDownloads",
    itemSelector: "#downloadsRichListBox richlistitem .downloadContainer",
    async userEvents(itemSelector, win) {
      let browser = win.gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:downloads");
      await contentTriggerDblclickOn(itemSelector, {}, browser);
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
];

function triggerDblclickOn(target, modifiers = {}, win) {
  let promise = BrowserTestUtils.waitForEvent(target, "dblclick");
  EventUtils.synthesizeMouseAtCenter(
    target,
    Object.assign({ clickCount: 1 }, modifiers),
    win
  );
  EventUtils.synthesizeMouseAtCenter(
    target,
    Object.assign({ clickCount: 2 }, modifiers),
    win
  );
  return promise;
}

function contentTriggerDblclickOn(selector, eventModifiers = {}, browser) {
  return SpecialPowers.spawn(
    browser,
    [selector, eventModifiers],
    async function(itemSelector, modifiers) {
      const EventUtils = ContentTaskUtils.getEventUtils(content);
      let itemTarget = content.document.querySelector(itemSelector);
      ok(itemTarget, "Download item target exists");

      let doubleClicked = ContentTaskUtils.waitForEvent(itemTarget, "dblclick");
      EventUtils.synthesizeMouseAtCenter(
        itemTarget,
        Object.assign({ clickCount: 1 }, modifiers),
        content
      );
      EventUtils.synthesizeMouseAtCenter(
        itemTarget,
        Object.assign({ clickCount: 2 }, modifiers),
        content
      );
      info("Waiting for double-click content task");
      await doubleClicked;
    }
  );
}

async function createDownloadedFile(pathname, contents) {
  let encoder = new TextEncoder();
  let file = new FileUtils.File(pathname);
  if (file.exists()) {
    info(`File at ${pathname} already exists`);
  }
  // No post-test cleanup necessary; tmp downloads directory is already removed after each test
  await OS.File.writeAtomic(pathname, encoder.encode(contents));
  ok(file.exists(), `Created ${pathname}`);
  return file;
}

async function addPDFDownload(itemData) {
  let startTimeMs = Date.now();

  let downloadPathname = OS.Path.join(gDownloadDir, itemData.targetFilename);
  info("Creating saved download file at:" + downloadPathname);
  let pdfFile = await createDownloadedFile(downloadPathname, DATA_PDF);
  info("Created file at:" + pdfFile.path);

  let downloadList = await Downloads.getList(Downloads.ALL);
  let download = {
    source: {
      url: "https://example.com/some.pdf",
    },
    target: {
      path: pdfFile.path,
    },
    succeeded: DownloadsCommon.DOWNLOAD_FINISHED,
    canceled: false,
    error: null,
    hasPartialData: false,
    hasBlockedData: itemData.hasBlockedData || false,
    startTime: new Date(startTimeMs++),
  };
  if (itemData.errorObj) {
    download.errorObj = itemData.errorObj;
  }

  await downloadList.add(await Downloads.createDownload(download));
  return download;
}

async function testSetup(testData = {}) {
  // Ensure that state is reset in case previous tests didn't finish.
  // remove download files, empty out collections
  info("Reset state");
  await task_resetState();
  if (gDownloadDir) {
    info("Removing existing download directory: " + gDownloadDir);
    await OS.File.removeDir(gDownloadDir, { ignoreAbsent: true });
  }
  gDownloadDir = await setDownloadDir();
  info("Created download directory: " + gDownloadDir);
}

async function openDownloadPanel(expectedItemCount) {
  // Open the user interface and wait for data to be fully loaded.
  let richlistbox = document.getElementById("downloadsListBox");
  await task_openPanel();
  await TestUtils.waitForCondition(
    () => richlistbox.childElementCount == expectedItemCount
  );
}

async function testOpenPDFPreview({
  name,
  whichUI,
  itemSelector,
  expected,
  userEvents,
}) {
  info("Test case: " + name);
  // Wait for focus first
  await promiseFocus();
  await testSetup();

  // Populate downloads database with the data required by this test.
  info("Adding download objects");
  let download = await addPDFDownload({
    targetFilename: "downloaded.pdf",
  });
  info("Got download pathname:" + download.target.path);

  let pdfFileURI = NetUtil.newURI(new FileUtils.File(download.target.path));
  info("pdfFileURI:" + pdfFileURI);

  let uiWindow = window;
  let initialTab = gBrowser.selectedTab;
  let previewWindow = window;
  let previewTab;
  let previewHappened;

  if (expected.newWindow) {
    info(
      "previewHappened will wait for new browser window with url: " +
        pdfFileURI.spec
    );
    // wait for a new browser window
    previewHappened = BrowserTestUtils.waitForNewWindow({
      anyWindow: true,
      url: pdfFileURI.spec,
    });
  } else {
    // wait for a tab to be opened
    info("previewHappened will wait for tab with URI:" + pdfFileURI.spec);
    previewHappened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      pdfFileURI.spec,
      false, // dont wait for load
      true // any tab, not just the next one
    );
  }

  let itemTarget;
  switch (whichUI) {
    case "downloadPanel":
      info("Opening download panel");
      await openDownloadPanel(expected.downloadCount);
      info("/Opening download panel");
      itemTarget = document.querySelector(itemSelector);
      break;
    case "allDownloads":
      // we'll be interacting with the library dialog
      uiWindow = await openLibrary("Downloads");

      let listbox = uiWindow.document.getElementById("downloadsRichListBox");
      ok(listbox, "download list box present");
      // wait for the expected number of items in the view
      await TestUtils.waitForCondition(
        () => listbox.itemChildren.length == expected.downloadCount
      );
      itemTarget = listbox.itemChildren[0];
      break;
    case "aboutDownloads":
      info(
        "in aboutDownloads, initially there are tabs: " + gBrowser.tabs.length
      );
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
      let browser = gBrowser.selectedBrowser;
      let downloadsLoaded = BrowserTestUtils.waitForEvent(
        browser,
        "InitialDownloadsLoaded",
        true
      );
      BrowserTestUtils.loadURI(browser, "about:downloads");
      info("waiting for downloadsLoaded");
      await downloadsLoaded;
      break;
  }

  info("Executing user events");
  info(
    "selectedBrowser has currentURI: " +
      gBrowser.selectedBrowser.currentURI.spec
  );
  await userEvents(itemTarget || itemSelector, uiWindow);
  info("/Executing user events");
  info("Waiting for previewHappened");
  let results = await previewHappened;
  if (expected.newWindow) {
    previewWindow = results;
    info("New window expected, got previewWindow? " + previewWindow);
  }
  previewTab =
    previewWindow.gBrowser.tabs[previewWindow.gBrowser.tabs.length - 1];
  ok(previewTab, "Got preview tab");

  let isSelected = previewWindow.gBrowser.selectedTab == previewTab;
  if (expected.tabSelected) {
    ok(isSelected, "The preview tab was selected");
  } else {
    ok(!isSelected, "The preview tab was opened in the background");
  }

  is(
    previewTab.linkedBrowser.currentURI.spec,
    pdfFileURI.spec,
    "previewTab has the expected currentURI"
  );

  info("cleaning up");
  if (whichUI == "downloadPanel") {
    DownloadsPanel.hidePanel();
  }

  info("Test opened a new UI window? " + (uiWindow !== window));
  if (uiWindow !== window) {
    await BrowserTestUtils.closeWindow(uiWindow);
  }
  if (expected.newWindow) {
    // will also close the previewTab
    await BrowserTestUtils.closeWindow(previewWindow);
  } else {
    await BrowserTestUtils.removeTab(previewTab);
  }
  if (gBrowser.selectedTab !== initialTab) {
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
}

// register the tests
for (let testData of TestCases) {
  if (testData.skip) {
    info("Skipping test:" + testData.name);
    continue;
  }
  // use the 'name' property of each test case as the test function name
  // so we get useful logs
  let tmp = {
    async [testData.name]() {
      await testOpenPDFPreview(testData);
    },
  };
  add_task(tmp[testData.name]);
}
