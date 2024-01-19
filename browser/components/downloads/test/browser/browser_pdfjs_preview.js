/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let gDownloadDir;

// The test is long, and it's not worth splitting it since all the tests share
// the same boilerplate code.
requestLongerTimeout(4);

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
    name: "Download panel, system viewer menu items prefd off",
    whichUI: "downloadPanel",
    itemSelector: "#downloadsListBox richlistitem .downloadMainArea",
    async userEvents(itemTarget, win) {
      EventUtils.synthesizeMouseAtCenter(itemTarget, {}, win);
    },
    prefs: [
      ["browser.download.openInSystemViewerContextMenuItem", false],
      ["browser.download.alwaysOpenInSystemViewerContextMenuItem", false],
    ],
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
      useSystemMenuItemDisabled: true,
      alwaysMenuItemDisabled: true,
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
    name: "Library all downloads dialog, system viewer menu items prefd off",
    whichUI: "allDownloads",
    async userEvents(itemTarget, win) {
      // double click
      await triggerDblclickOn(itemTarget, {}, win);
    },
    prefs: [
      ["browser.download.openInSystemViewerContextMenuItem", false],
      ["browser.download.alwaysOpenInSystemViewerContextMenuItem", false],
    ],
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
      useSystemMenuItemDisabled: true,
      alwaysMenuItemDisabled: true,
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
    itemSelector: "#downloadsListBox richlistitem .downloadContainer",
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
    name: "about:downloads, system viewer menu items prefd off",
    whichUI: "aboutDownloads",
    itemSelector: "#downloadsListBox richlistitem .downloadContainer",
    async userEvents(itemSelector, win) {
      let browser = win.gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:downloads");
      await contentTriggerDblclickOn(itemSelector, {}, browser);
    },
    prefs: [
      ["browser.download.openInSystemViewerContextMenuItem", false],
      ["browser.download.alwaysOpenInSystemViewerContextMenuItem", false],
    ],
    expected: {
      downloadCount: 1,
      newWindow: false,
      opensTab: true,
      tabSelected: true,
      useSystemMenuItemDisabled: true,
      alwaysMenuItemDisabled: true,
    },
  },
  {
    name: "about:downloads, open in new window",
    whichUI: "aboutDownloads",
    itemSelector: "#downloadsListBox richlistitem .downloadContainer",
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
    itemSelector: "#downloadsListBox richlistitem .downloadContainer",
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
    itemSelector: "#downloadsListBox richlistitem .downloadContainer",
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
    itemSelector: "#downloadsListBox richlistitem .downloadContainer",
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
    async function (itemSelector, modifiers) {
      const EventUtils = ContentTaskUtils.getEventUtils(content);
      let itemTarget = content.document.querySelector(itemSelector);
      ok(itemTarget, "Download item target exists");

      let doubleClicked = ContentTaskUtils.waitForEvent(itemTarget, "dblclick");
      // NOTE: we are using sendMouseEvent instead of synthesizeMouseAtCenter
      // here to prevent an unexpected timeout failure in devedition builds
      // due to the ContentTaskUtils.waitForEvent promise never been resolved.
      EventUtils.sendMouseEvent(
        { type: "dblclick", ...modifiers },
        itemTarget,
        content
      );
      info("Waiting for double-click content task");
      await doubleClicked;
    }
  );
}

async function verifyContextMenu(contextMenu, expected = {}) {
  info("verifyContextMenu with expected: " + JSON.stringify(expected, null, 2));
  let alwaysMenuItem = contextMenu.querySelector(
    ".downloadAlwaysUseSystemDefaultMenuItem"
  );
  let useSystemMenuItem = contextMenu.querySelector(
    ".downloadUseSystemDefaultMenuItem"
  );
  info("Waiting for the context menu to show up");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(contextMenu),
    "The context menu is visible"
  );
  await TestUtils.waitForTick();

  info("Checking visibility of the system viewer menu items");
  is(
    BrowserTestUtils.isHidden(useSystemMenuItem),
    expected.useSystemMenuItemDisabled,
    `The 'Use system viewer' menu item was ${
      expected.useSystemMenuItemDisabled ? "hidden" : "visible"
    }`
  );
  is(
    BrowserTestUtils.isHidden(alwaysMenuItem),
    expected.alwaysMenuItemDisabled,
    `The 'Use system viewer' menu item was ${
      expected.alwaysMenuItemDisabled ? "hidden" : "visible"
    }`
  );

  if (!expected.useSystemMenuItemDisabled && expected.alwaysChecked) {
    is(
      alwaysMenuItem.getAttribute("checked"),
      "true",
      "The 'Always...' menu item is checked"
    );
  } else if (!expected.useSystemMenuItemDisabled) {
    ok(
      !alwaysMenuItem.hasAttribute("checked"),
      "The 'Always...' menu item not checked"
    );
  }
}

async function addPDFDownload(itemData) {
  let startTimeMs = Date.now();
  info("addPDFDownload with itemData: " + JSON.stringify(itemData, null, 2));

  let downloadPathname = PathUtils.join(gDownloadDir, itemData.targetFilename);
  delete itemData.targetFilename;

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
    ...itemData,
  };
  if (itemData.errorObj) {
    download.errorObj = itemData.errorObj;
  }

  await downloadList.add(await Downloads.createDownload(download));
  return download;
}

async function testSetup() {
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
    () =>
      richlistbox.childElementCount == expectedItemCount &&
      !richlistbox.getAttribute("disabled")
  );
}

async function testOpenPDFPreview({
  name,
  whichUI,
  downloadProperties,
  itemSelector,
  expected,
  prefs = [],
  userEvents,
  isPrivate,
}) {
  info("Test case: " + name);
  // Wait for focus first
  await promiseFocus();
  await testSetup();
  if (prefs.length) {
    await SpecialPowers.pushPrefEnv({
      set: prefs,
    });
  }

  // Populate downloads database with the data required by this test.
  info("Adding download objects");
  if (!downloadProperties) {
    downloadProperties = {
      targetFilename: "downloaded.pdf",
    };
  }
  let download = await addPDFDownload({
    ...downloadProperties,
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
    let contextMenu;

    switch (whichUI) {
      case "downloadPanel":
        info("Opening download panel");
        await openDownloadPanel(expected.downloadCount);
        info("/Opening download panel");
        itemTarget = document.querySelector(itemSelector);
        contextMenu = uiWindow.document.querySelector("#downloadsContextMenu");

        break;
      case "allDownloads":
        // we'll be interacting with the library dialog
        uiWindow = await openLibrary("Downloads");

        let listbox = uiWindow.document.getElementById("downloadsListBox");
        ok(listbox, "download list box present");
        // wait for the expected number of items in the view,
        // and for the first item to be visible && clickable
        await TestUtils.waitForCondition(() => {
          return (
            listbox.itemChildren.length == expected.downloadCount &&
            BrowserTestUtils.isVisible(listbox.itemChildren[0])
          );
        });
        itemTarget = listbox.itemChildren[0];
        contextMenu = uiWindow.document.querySelector("#downloadsContextMenu");

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
        BrowserTestUtils.startLoadingURIString(browser, "about:downloads");
        await BrowserTestUtils.browserLoaded(browser);
        info("waiting for downloadsLoaded");
        await downloadsLoaded;

        await ContentTask.spawn(
          browser,
          [expected.downloadCount],
          async function awaitListItems(expectedCount) {
            await ContentTaskUtils.waitForCondition(
              () =>
                content.document.getElementById("downloadsListBox")
                  .childElementCount == expectedCount,
              `Await ${expectedCount} download list items`
            );
          }
        );
        break;
    }

    if (contextMenu) {
      info("trigger the contextmenu");
      await openContextMenu(itemTarget || itemSelector, uiWindow);
      info("context menu should be open, verify its menu items");
      let expectedValues = {
        useSystemMenuItemDisabled: false,
        alwaysMenuItemDisabled: false,
        ...expected,
      };
      await verifyContextMenu(contextMenu, expectedValues);
      contextMenu.hidePopup();
    } else {
      todo(contextMenu, "No context menu checks for test: " + name);
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
  if (prefs.length) {
    await SpecialPowers.popPrefEnv();
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
