/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const DATA_PDF = atob(
  "JVBERi0xLjANCjEgMCBvYmo8PC9UeXBlL0NhdGFsb2cvUGFnZXMgMiAwIFI+PmVuZG9iaiAyIDAgb2JqPDwvVHlwZS9QYWdlcy9LaWRzWzMgMCBSXS9Db3VudCAxPj5lbmRvYmogMyAwIG9iajw8L1R5cGUvUGFnZS9NZWRpYUJveFswIDAgMyAzXT4+ZW5kb2JqDQp4cmVmDQowIDQNCjAwMDAwMDAwMDAgNjU1MzUgZg0KMDAwMDAwMDAxMCAwMDAwMCBuDQowMDAwMDAwMDUzIDAwMDAwIG4NCjAwMDAwMDAxMDIgMDAwMDAgbg0KdHJhaWxlcjw8L1NpemUgNC9Sb290IDEgMCBSPj4NCnN0YXJ0eHJlZg0KMTQ5DQolRU9G"
);
let gDownloadDir;

SimpleTest.requestFlakyTimeout(
  "Giving a chance for possible last-pb-context-exited to occur (Bug 1329912)"
);

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
    name: "Download panel, open in new window",
    whichUI: "downloadPanel",
    itemSelector: "#downloadsListBox richlistitem .downloadMainArea",
    async userEvents(itemTarget, win) {
      EventUtils.synthesizeMouseAtCenter(itemTarget, { shiftKey: true }, win);
    },
    expected: {
      downloadCount: 1,
      newWindow: true,
      opensTab: false,
      tabSelected: true,
    },
  },
  {
    name: "Download panel, open foreground tab", // duplicates the default behavior
    whichUI: "downloadPanel",
    itemSelector: "#downloadsListBox richlistitem .downloadMainArea",
    async userEvents(itemTarget, win) {
      EventUtils.synthesizeMouseAtCenter(
        itemTarget,
        { ctrlKey: true, metaKey: true },
        win
      );
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
  {
    name: "Download panel, open background tab",
    whichUI: "downloadPanel",
    itemSelector: "#downloadsListBox richlistitem .downloadMainArea",
    async userEvents(itemTarget, win) {
      EventUtils.synthesizeMouseAtCenter(
        itemTarget,
        { ctrlKey: true, metaKey: true, shiftKey: true },
        win
      );
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: false,
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
    name: "Library all downloads dialog, open from keyboard",
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
    name: "Library all downloads dialog, open in new window",
    whichUI: "allDownloads",
    async userEvents(itemTarget, win) {
      // double click
      await triggerDblclickOn(itemTarget, { shiftKey: true }, win);
    },
    expected: {
      downloadCount: 1,
      newWindow: true,
      opensTab: false,
      tabSelected: true,
    },
  },
  {
    name: "Library all downloads dialog, open foreground tab", // duplicates default behavior
    whichUI: "allDownloads",
    async userEvents(itemTarget, win) {
      // double click
      await triggerDblclickOn(
        itemTarget,
        { ctrlKey: true, metaKey: true },
        win
      );
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
  {
    name: "Library all downloads dialog, open background tab",
    whichUI: "allDownloads",
    async userEvents(itemTarget, win) {
      // double click
      await triggerDblclickOn(
        itemTarget,
        { ctrlKey: true, metaKey: true, shiftKey: true },
        win
      );
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: false,
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
  {
    name: "about:downloads, open in new window",
    whichUI: "aboutDownloads",
    itemSelector: "#downloadsRichListBox richlistitem .downloadContainer",
    async userEvents(itemSelector, win) {
      let browser = win.gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:downloads");
      await contentTriggerDblclickOn(itemSelector, { shiftKey: true }, browser);
    },
    expected: {
      downloadCount: 1,
      newWindow: true,
      opensTab: false,
      tabSelected: true,
    },
  },
  {
    name: "about:downloads, open in foreground tab",
    whichUI: "aboutDownloads",
    itemSelector: "#downloadsRichListBox richlistitem .downloadContainer",
    async userEvents(itemSelector, win) {
      let browser = win.gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:downloads");
      await contentTriggerDblclickOn(
        itemSelector,
        { ctrlKey: true, metaKey: true },
        browser
      );
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
    },
  },
  {
    name: "about:downloads, open in background tab",
    whichUI: "aboutDownloads",
    itemSelector: "#downloadsRichListBox richlistitem .downloadContainer",
    async userEvents(itemSelector, win) {
      let browser = win.gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:downloads");
      await contentTriggerDblclickOn(
        itemSelector,
        { ctrlKey: true, metaKey: true, shiftKey: true },
        browser
      );
    },
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: false,
    },
  },
  {
    name: "Private download in about:downloads, opens in new private window",
    skip: true, // Bug 1641770
    whichUI: "aboutDownloads",
    itemSelector: "#downloadsRichListBox richlistitem .downloadContainer",
    async userEvents(itemSelector, win) {
      let browser = win.gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:downloads");
      await contentTriggerDblclickOn(itemSelector, { shiftKey: true }, browser);
    },
    isPrivate: true,
    expected: {
      downloadCount: 1,
      newWindow: true,
      opensTab: false,
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

  let downloadList = await Downloads.getList(
    itemData.isPrivate ? Downloads.PRIVATE : Downloads.PUBLIC
  );
  let download = {
    source: {
      url: "https://example.com/some.pdf",
      isPrivate: itemData.isPrivate,
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
  // remove download files, empty out collections
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  is(downloadCount, 0, "At the start of the test, there should be 0 downloads");

  await task_resetState();
  if (!gDownloadDir) {
    gDownloadDir = await setDownloadDir();
  }
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
  isPrivate,
}) {
  info("Test case: " + name);
  // Wait for focus first
  await promiseFocus();
  await testSetup();

  // Populate downloads database with the data required by this test.
  info("Adding download objects");
  let download = await addPDFDownload({
    targetFilename: "downloaded.pdf",
    isPrivate,
  });
  info("Got download pathname:" + download.target.path);
  is(
    !!download.source.isPrivate,
    !!isPrivate,
    `Added download is ${isPrivate ? "private" : "not private"} as expected`
  );
  let downloadList = await Downloads.getList(
    isPrivate ? Downloads.PRIVATE : Downloads.PUBLIC
  );
  let downloads = await downloadList.getAll();
  is(
    downloads.length,
    expected.downloadCount,
    `${isPrivate ? "Private" : "Public"} list has expected ${
      downloads.length
    } downloads`
  );

  let pdfFileURI = NetUtil.newURI(new FileUtils.File(download.target.path));
  info("pdfFileURI:" + pdfFileURI.spec);

  let uiWindow = window;
  let previewWindow = window;
  // we never want to unload the test browser by loading the file: URI into it
  await BrowserTestUtils.withNewTab("about:blank", async initialBrowser => {
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
    } else if (expected.opensTab) {
      // wait for a tab to be opened
      info("previewHappened will wait for tab with URI:" + pdfFileURI.spec);
      previewHappened = BrowserTestUtils.waitForNewTab(
        gBrowser,
        pdfFileURI.spec,
        false, // dont wait for load
        true // any tab, not just the next one
      );
    } else {
      info(
        "previewHappened will wait to load " +
          pdfFileURI.spec +
          " into the current tab"
      );
      previewHappened = BrowserTestUtils.browserLoaded(
        initialBrowser,
        false,
        pdfFileURI.spec
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
        info("Preparing about:downloads browser window");

        // Because of bug 1329912, we sometimes get a bogus last-pb-context-exited notification
        // which removes all the private downloads and about:downloads renders a empty list
        // we'll allow time for that to happen before loading about:downloads
        let pbExitedOrTimeout = isPrivate
          ? new Promise(resolve => {
              const topic = "last-pb-context-exited";
              const ENOUGH_TIME = 1000;
              function observer() {
                info(`Bogus ${topic} observed`);
                done();
              }
              function done() {
                clearTimeout(timerId);
                Services.obs.removeObserver(observer, topic);
                resolve();
              }
              /* eslint-disable mozilla/no-arbitrary-setTimeout */
              const timerId = setTimeout(done, ENOUGH_TIME);
              Services.obs.addObserver(observer, "last-pb-context-exited");
            })
          : Promise.resolve();

        if (isPrivate) {
          uiWindow = await BrowserTestUtils.openNewBrowserWindow({
            private: true,
          });
        }
        info(
          "in aboutDownloads, initially there are tabs: " +
            uiWindow.gBrowser.tabs.length
        );

        let browser = uiWindow.gBrowser.selectedBrowser;
        await pbExitedOrTimeout;

        info("Loading about:downloads");
        let downloadsLoaded = BrowserTestUtils.waitForEvent(
          browser,
          "InitialDownloadsLoaded",
          true
        );
        BrowserTestUtils.loadURI(browser, "about:downloads");
        await BrowserTestUtils.browserLoaded(browser);
        info("waiting for downloadsLoaded");
        await downloadsLoaded;

        await ContentTask.spawn(
          browser,
          [expected.downloadCount],
          async function awaitListItems(expectedCount) {
            await ContentTaskUtils.waitForCondition(
              () =>
                content.document.getElementById("downloadsRichListBox")
                  .childElementCount == expectedCount,
              `Await ${expectedCount} download list items`
            );
          }
        );
        break;
    }

    info("Executing user events");
    await userEvents(itemTarget || itemSelector, uiWindow);

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

    is(
      PrivateBrowsingUtils.isBrowserPrivate(previewTab.linkedBrowser),
      !!isPrivate,
      `The preview tab was ${isPrivate ? "private" : "not private"} as expected`
    );

    info("cleaning up");
    if (whichUI == "downloadPanel") {
      DownloadsPanel.hidePanel();
    }
    let lastPBContextExitedPromise = isPrivate
      ? TestUtils.topicObserved("last-pb-context-exited").then(() =>
          TestUtils.waitForTick()
        )
      : Promise.resolve();

    info("Test opened a new UI window? " + (uiWindow !== window));
    if (uiWindow !== window) {
      info("Closing uiWindow");
      await BrowserTestUtils.closeWindow(uiWindow);
    }
    if (expected.newWindow) {
      // will also close the previewTab
      await BrowserTestUtils.closeWindow(previewWindow);
    } else {
      await BrowserTestUtils.removeTab(previewTab);
    }
    info("Waiting for lastPBContextExitedPromise");
    await lastPBContextExitedPromise;
  });
  await downloadList.removeFinished();
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
