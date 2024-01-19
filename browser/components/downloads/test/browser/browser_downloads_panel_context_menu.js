/*
  Coverage for context menu state for downloads in the Downloads Panel
*/

let gDownloadDir;
const TestFiles = {};

let ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

// Load a new URI with a specific referrer.
let exampleRefInfo = new ReferrerInfo(
  Ci.nsIReferrerInfo.EMPTY,
  true,
  Services.io.newURI("https://example.org")
);

const MENU_ITEMS = {
  pause: ".downloadPauseMenuItem",
  resume: ".downloadResumeMenuItem",
  unblock: '[command="downloadsCmd_unblock"]',
  openInSystemViewer: '[command="downloadsCmd_openInSystemViewer"]',
  alwaysOpenInSystemViewer: '[command="downloadsCmd_alwaysOpenInSystemViewer"]',
  alwaysOpenSimilarFiles: '[command="downloadsCmd_alwaysOpenSimilarFiles"]',
  show: '[command="downloadsCmd_show"]',
  commandsSeparator: "menuseparator,.downloadCommandsSeparator",
  openReferrer: ".downloadOpenReferrerMenuItem",
  copyLocation: ".downloadCopyLocationMenuItem",
  separator: "menuseparator",
  deleteFile: ".downloadDeleteFileMenuItem",
  delete: '[command="cmd_delete"]',
  clearList: '[command="downloadsCmd_clearList"]',
  clearDownloads: '[command="downloadsCmd_clearDownloads"]',
};

const TestCasesNewMimetypes = [
  {
    name: "Completed txt download",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_FINISHED,
        contentType: "text/plain",
        target: {},
        source: {
          referrerInfo: exampleRefInfo,
        },
      },
    ],
    expected: {
      menu: [
        MENU_ITEMS.alwaysOpenSimilarFiles,
        MENU_ITEMS.show,
        MENU_ITEMS.commandsSeparator,
        MENU_ITEMS.openReferrer,
        MENU_ITEMS.copyLocation,
        MENU_ITEMS.separator,
        MENU_ITEMS.deleteFile,
        MENU_ITEMS.delete,
        MENU_ITEMS.clearList,
      ],
    },
  },
  {
    name: "Canceled txt download",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_CANCELED,
        contentType: "text/plain",
        target: {},
        source: {
          referrerInfo: exampleRefInfo,
        },
      },
    ],
    expected: {
      menu: [
        MENU_ITEMS.openReferrer,
        MENU_ITEMS.copyLocation,
        MENU_ITEMS.separator,
        MENU_ITEMS.delete,
        MENU_ITEMS.clearList,
      ],
    },
  },
  {
    name: "Completed unknown ext download with application/octet-stream",
    overrideExtension: "unknownExtension",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_FINISHED,
        contentType: "application/octet-stream",
        target: {},
        source: {
          referrerInfo: exampleRefInfo,
        },
      },
    ],
    expected: {
      menu: [
        MENU_ITEMS.show,
        MENU_ITEMS.commandsSeparator,
        MENU_ITEMS.openReferrer,
        MENU_ITEMS.copyLocation,
        MENU_ITEMS.separator,
        MENU_ITEMS.deleteFile,
        MENU_ITEMS.delete,
        MENU_ITEMS.clearList,
      ],
    },
  },
  {
    name: "Completed txt download with application/octet-stream",
    overrideExtension: "txt",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_FINISHED,
        contentType: "application/octet-stream",
        target: {},
        source: {
          referrerInfo: exampleRefInfo,
        },
      },
    ],
    expected: {
      menu: [
        // Despite application/octet-stream content type, ensure
        // alwaysOpenSimilarFiles still appears since txt files
        // are supported file types.
        MENU_ITEMS.alwaysOpenSimilarFiles,
        MENU_ITEMS.show,
        MENU_ITEMS.commandsSeparator,
        MENU_ITEMS.openReferrer,
        MENU_ITEMS.copyLocation,
        MENU_ITEMS.separator,
        MENU_ITEMS.deleteFile,
        MENU_ITEMS.delete,
        MENU_ITEMS.clearList,
      ],
    },
  },
];

const TestCasesDeletedFile = [
  {
    name: "Download with file deleted",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_FINISHED,
        contentType: "text/plain",
        target: {},
        source: {
          referrerInfo: exampleRefInfo,
        },
        deleted: true,
      },
    ],
    expected: {
      menu: [
        MENU_ITEMS.alwaysOpenSimilarFiles,
        MENU_ITEMS.openReferrer,
        MENU_ITEMS.copyLocation,
        MENU_ITEMS.separator,
        MENU_ITEMS.delete,
        MENU_ITEMS.clearList,
      ],
    },
  },
];

const TestCasesMultipleFiles = [
  {
    name: "Multiple files",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_FINISHED,
        contentType: "text/plain",
        target: {},
        source: {
          referrerInfo: exampleRefInfo,
        },
      },
      {
        state: DownloadsCommon.DOWNLOAD_FINISHED,
        contentType: "text/plain",
        target: {},
        source: {
          referrerInfo: exampleRefInfo,
        },
        deleted: true,
      },
    ],
    expected: {
      menu: [
        MENU_ITEMS.alwaysOpenSimilarFiles,
        MENU_ITEMS.openReferrer,
        MENU_ITEMS.copyLocation,
        MENU_ITEMS.separator,
        MENU_ITEMS.delete,
        MENU_ITEMS.clearList,
      ],
    },
    itemIndex: 1,
  },
];

add_setup(async function () {
  // remove download files, empty out collections
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  is(downloadCount, 0, "At the start of the test, there should be 0 downloads");

  await task_resetState();
  if (!gDownloadDir) {
    gDownloadDir = await setDownloadDir();
  }
  info("Created download directory: " + gDownloadDir);

  // create the downloaded files we'll need
  TestFiles.pdf = await createDownloadedFile(
    PathUtils.join(gDownloadDir, "downloaded.pdf"),
    DATA_PDF
  );
  info("Created downloaded PDF file at:" + TestFiles.pdf.path);
  TestFiles.txt = await createDownloadedFile(
    PathUtils.join(gDownloadDir, "downloaded.txt"),
    "Test file"
  );
  info("Created downloaded text file at:" + TestFiles.txt.path);
  TestFiles.unknownExtension = await createDownloadedFile(
    PathUtils.join(gDownloadDir, "downloaded.unknownExtension"),
    "Test file"
  );
  info(
    "Created downloaded unknownExtension file at:" +
      TestFiles.unknownExtension.path
  );
  TestFiles.nonexistentFile = new FileUtils.File(
    PathUtils.join(gDownloadDir, "nonexistent")
  );
  info(
    "Created nonexistent downloaded file at:" + TestFiles.nonexistentFile.path
  );
});

// non default mimetypes
for (let testData of TestCasesNewMimetypes) {
  if (testData.skip) {
    info("Skipping test:" + testData.name);
    continue;
  }
  // use the 'name' property of each test case as the test function name
  // so we get useful logs
  let tmp = {
    async [testData.name]() {
      await testDownloadContextMenu(testData);
    },
  };
  add_task(tmp[testData.name]);
}

for (let testData of TestCasesDeletedFile) {
  if (testData.skip) {
    info("Skipping test:" + testData.name);
    continue;
  }
  // use the 'name' property of each test case as the test function name
  // so we get useful logs
  let tmp = {
    async [testData.name]() {
      await testDownloadContextMenu(testData);
    },
  };
  add_task(tmp[testData.name]);
}

for (let testData of TestCasesMultipleFiles) {
  if (testData.skip) {
    info("Skipping test:" + testData.name);
    continue;
  }
  // use the 'name' property of each test case as the test function name
  // so we get useful logs
  let tmp = {
    async [testData.name]() {
      await testDownloadContextMenu(testData);
    },
  };
  add_task(tmp[testData.name]);
}

async function testDownloadContextMenu({
  overrideExtension = null,
  downloads = [],
  expected,
  itemIndex = 0,
}) {
  // prepare downloads
  await prepareDownloads(downloads, overrideExtension);
  let downloadList = await Downloads.getList(Downloads.PUBLIC);
  let download = (await downloadList.getAll())[itemIndex];
  info("Download succeeded? " + download.succeeded);
  info("Download target exists? " + download.target.exists);

  // open panel
  await task_openPanel();
  await TestUtils.waitForCondition(() => {
    let downloadsListBox = document.getElementById("downloadsListBox");
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == downloads.length;
  });

  let itemTarget = document
    .querySelectorAll("#downloadsListBox richlistitem")
    [itemIndex].querySelector(".downloadMainArea");
  EventUtils.synthesizeMouse(itemTarget, 1, 1, { type: "mousemove" });
  is(
    DownloadsView.richListBox.selectedIndex,
    0,
    "moving the mouse resets the richlistbox's selected index"
  );

  info("trigger the context menu");
  let contextMenu = await openContextMenu(itemTarget);

  // FIXME: This works in practice, but simulating the context menu opening
  // doesn't seem to automatically set the selected index.
  DownloadsView.richListBox.selectedIndex = itemIndex;
  EventUtils.synthesizeMouse(itemTarget, 1, 1, { type: "mousemove" });
  is(
    DownloadsView.richListBox.selectedIndex,
    itemIndex,
    "selected index after opening the context menu and moving the mouse"
  );

  info("context menu should be open, verify its menu items");
  let result = verifyContextMenu(contextMenu, expected.menu);

  // close menus
  contextMenu.hidePopup();
  let hiddenPromise = BrowserTestUtils.waitForEvent(
    DownloadsPanel.panel,
    "popuphidden"
  );
  DownloadsPanel.hidePanel();
  await hiddenPromise;

  ok(!result, "Expected no errors verifying context menu items");

  // clean up downloads
  await downloadList.removeFinished();
}

// ----------------------------------------------------------------------------
// Helpers

function verifyContextMenu(contextMenu, itemSelectors) {
  // Ignore hidden nodes
  let items = Array.from(contextMenu.children).filter(n =>
    BrowserTestUtils.isVisible(n)
  );
  let menuAsText = items
    .map(n => {
      return n.nodeName == "menuseparator"
        ? "---"
        : `${n.label} (${n.command})`;
    })
    .join("\n");
  info("Got actual context menu items: \n" + menuAsText);

  try {
    is(
      items.length,
      itemSelectors.length,
      "Context menu has the expected number of items"
    );
    for (let i = 0; i < items.length; i++) {
      let selector = itemSelectors[i];
      ok(
        items[i].matches(selector),
        `Item at ${i} matches expected selector: ${selector}`
      );
    }
  } catch (ex) {
    return ex;
  }
  return null;
}

async function prepareDownloads(downloads, overrideExtension = null) {
  for (let props of downloads) {
    info(JSON.stringify(props));
    if (props.state !== DownloadsCommon.DOWNLOAD_FINISHED) {
      continue;
    }
    if (props.deleted) {
      props.target = TestFiles.nonexistentFile;
      continue;
    }
    switch (props.contentType) {
      case "application/pdf":
        props.target = TestFiles.pdf;
        break;
      case "text/plain":
        props.target = TestFiles.txt;
        break;
      case "application/octet-stream":
        props.target = TestFiles[overrideExtension];
        break;
    }
    ok(props.target instanceof Ci.nsIFile, "download target is a nsIFile");
  }
  // If we'd just insert downloads as defined in the test case, they would
  // appear reversed in the panel, because they will be in descending insertion
  // order (newest at the top). The problem is we define an itemIndex based on
  // the downloads array, and it would be weird to define it based on a
  // reversed order. Short, we just reverse the array to preserve the order.
  await task_addDownloads(downloads.reverse());
}
