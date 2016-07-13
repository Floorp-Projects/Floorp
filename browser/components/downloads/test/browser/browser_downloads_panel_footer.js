"use strict";

function *task_openDownloadsSubPanel() {
  let downloadSubPanel = document.getElementById("downloadSubPanel");
  let popupShownPromise = BrowserTestUtils.waitForEvent(downloadSubPanel, "popupshown");

  let downloadsDropmarker = document.getElementById("downloadsFooterDropmarker");
  EventUtils.synthesizeMouseAtCenter(downloadsDropmarker, {}, window);

  yield popupShownPromise;
}

add_task(function* test_openDownloadsFolder() {
  yield task_openPanel();

  yield task_openDownloadsSubPanel();

  yield new Promise(resolve => {
    sinon.stub(DownloadsCommon, "showDirectory", file => {
      resolve(Downloads.getPreferredDownloadsDirectory().then(downloadsPath => {
        is(file.path, downloadsPath, "Check the download folder path.");
      }));
    });

    let itemOpenDownloadsFolder =
      document.getElementById("downloadsDropdownItemOpenDownloadsFolder");
    EventUtils.synthesizeMouseAtCenter(itemOpenDownloadsFolder, {}, window);
  });

  yield task_resetState();
});

add_task(function* test_clearList() {
  const kTestCases = [{
    downloads: [
      { state: nsIDM.DOWNLOAD_NOTSTARTED },
      { state: nsIDM.DOWNLOAD_FINISHED },
      { state: nsIDM.DOWNLOAD_FAILED },
      { state: nsIDM.DOWNLOAD_CANCELED },
    ],
    expectClearListShown: true,
    expectedItemNumber: 0,
  },{
    downloads: [
      { state: nsIDM.DOWNLOAD_NOTSTARTED },
      { state: nsIDM.DOWNLOAD_FINISHED },
      { state: nsIDM.DOWNLOAD_FAILED },
      { state: nsIDM.DOWNLOAD_PAUSED },
      { state: nsIDM.DOWNLOAD_CANCELED },
    ],
    expectClearListShown: true,
    expectedItemNumber: 1,
  },{
    downloads: [
      { state: nsIDM.DOWNLOAD_PAUSED },
    ],
    expectClearListShown: false,
    expectedItemNumber: 1,
  }];

  for (let testCase of kTestCases) {
    yield verify_clearList(testCase);
  }
});

function *verify_clearList(testCase) {
  let downloads = testCase.downloads;
  yield task_addDownloads(downloads);

  yield task_openPanel();
  is(DownloadsView._downloads.length, downloads.length,
    "Expect the number of download items");

  yield task_openDownloadsSubPanel();

  let itemClearList = document.getElementById("downloadsDropdownItemClearList");
  let itemNumberPromise = BrowserTestUtils.waitForCondition(() => {
    return DownloadsView._downloads.length === testCase.expectedItemNumber;
  });
  if (testCase.expectClearListShown) {
    isnot("true", itemClearList.getAttribute("hidden"),
      "Should show Clear Preview Panel button");
    EventUtils.synthesizeMouseAtCenter(itemClearList, {}, window);
  } else {
    is("true", itemClearList.getAttribute("hidden"),
      "Should not show Clear Preview Panel button");
  }

  yield itemNumberPromise;
  is(DownloadsView._downloads.length, testCase.expectedItemNumber,
    "Download items remained.");

  yield task_resetState();
}
