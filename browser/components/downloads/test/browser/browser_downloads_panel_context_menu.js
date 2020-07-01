/*
  Coverage for context menu state for downloads in the Downloads Panel
*/

let gDownloadDir;
const TestFiles = {};

const MENU_ITEMS = {
  pause: ".downloadPauseMenuItem",
  resume: ".downloadResumeMenuItem",
  unblock: '[command="downloadsCmd_unblock"]',
  openInSystemViewer: '[command="downloadsCmd_openInSystemViewer"]',
  alwaysOpenInSystemViewer: '[command="downloadsCmd_alwaysOpenInSystemViewer"]',
  show: '[command="downloadsCmd_show"]',
  commandsSeparator: "menuseparator,.downloadCommandsSeparator",
  openReferrer: '[command="downloadsCmd_openReferrer"]',
  copyLocation: '[command="downloadsCmd_copyLocation"]',
  separator: "menuseparator",
  delete: '[command="cmd_delete"]',
  clearList: '[command="downloadsCmd_clearList"]',
  clearDownloads: '[command="downloadsCmd_clearDownloads"]',
};

const TestCases = [
  {
    name: "Completed PDF download",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_FINISHED,
        contentType: "application/pdf",
        target: {},
      },
    ],
    expected: {
      menu: [
        MENU_ITEMS.openInSystemViewer,
        MENU_ITEMS.alwaysOpenInSystemViewer,
        MENU_ITEMS.show,
        MENU_ITEMS.commandsSeparator,
        MENU_ITEMS.openReferrer,
        MENU_ITEMS.copyLocation,
        MENU_ITEMS.separator,
        MENU_ITEMS.delete,
        MENU_ITEMS.clearList,
      ],
    },
  },
  {
    name: "Canceled PDF download",
    downloads: [
      {
        state: DownloadsCommon.DOWNLOAD_CANCELED,
        contentType: "application/pdf",
        target: {},
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
];

add_task(async function test_setUp() {
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
    OS.Path.join(gDownloadDir, "downloaded.pdf"),
    DATA_PDF
  );
  info("Created downloaded PDF file at:" + TestFiles.pdf.path);
  TestFiles.txt = await createDownloadedFile(
    OS.Path.join(gDownloadDir, "downloaded.txt"),
    "Test file"
  );
  info("Created downloaded text file at:" + TestFiles.txt.path);
});

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
      await testDownloadContextMenu(testData);
    },
  };
  add_task(tmp[testData.name]);
}

async function testDownloadContextMenu({ downloads = [], expected }) {
  // prepare downloads
  await prepareDownloads(downloads);
  let downloadList = await Downloads.getList(Downloads.PUBLIC);
  let [firstDownload] = await downloadList.getAll();
  info("Download succeeded? " + firstDownload.succeeded);
  info("Download target exists? " + firstDownload.target.exists);

  // open panel
  await task_openPanel();
  await TestUtils.waitForCondition(
    () =>
      document.getElementById("downloadsListBox").childElementCount ==
      downloads.length
  );

  info("trigger the context menu");
  let itemTarget = document.querySelector(
    "#downloadsListBox richlistitem .downloadMainArea"
  );

  let contextMenu = await openContextMenu(itemTarget);

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
    BrowserTestUtils.is_visible(n)
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

async function prepareDownloads(downloads) {
  for (let props of downloads) {
    info(JSON.stringify(props));
    if (props.state !== DownloadsCommon.DOWNLOAD_FINISHED) {
      continue;
    }
    switch (props.contentType) {
      case "application/pdf":
        props.target = TestFiles.pdf;
        break;
      default:
        props.target = TestFiles.txt;
        break;
    }
    ok(props.target instanceof Ci.nsIFile, "download target is a nsIFile");
  }
  await task_addDownloads(downloads);
}
