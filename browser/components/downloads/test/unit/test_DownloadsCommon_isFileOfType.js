const DATA_PDF = atob(
  "JVBERi0xLjANCjEgMCBvYmo8PC9UeXBlL0NhdGFsb2cvUGFnZXMgMiAwIFI+PmVuZG9iaiAyIDAgb2JqPDwvVHlwZS9QYWdlcy9LaWRzWzMgMCBSXS9Db3VudCAxPj5lbmRvYmogMyAwIG9iajw8L1R5cGUvUGFnZS9NZWRpYUJveFswIDAgMyAzXT4+ZW5kb2JqDQp4cmVmDQowIDQNCjAwMDAwMDAwMDAgNjU1MzUgZg0KMDAwMDAwMDAxMCAwMDAwMCBuDQowMDAwMDAwMDUzIDAwMDAwIG4NCjAwMDAwMDAxMDIgMDAwMDAgbg0KdHJhaWxlcjw8L1NpemUgNC9Sb290IDEgMCBSPj4NCnN0YXJ0eHJlZg0KMTQ5DQolRU9G"
);

const DOWNLOAD_TEMPLATE = {
  source: {
    url: "https://download-test.com/download",
  },
  target: {
    path: "",
  },
  contentType: "text/plain",
  succeeded: DownloadsCommon.DOWNLOAD_FINISHED,
  canceled: false,
  error: null,
  hasPartialData: false,
  hasBlockedData: false,
  startTime: new Date(Date.now() - 1000),
};

const TESTFILES = {
  "download-test.pdf": DATA_PDF,
  "download-test.xxunknown": DATA_PDF,
  "download-test-missing.pdf": null,
};
let gPublicList;
add_task(async function test_setup() {
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
  Assert.ok(profileDir, "profileDir: " + profileDir);
  for (let [filename, contents] of Object.entries(TESTFILES)) {
    TESTFILES[filename] = await createDownloadedFile(
      PathUtils.join(gDownloadDir, filename),
      contents
    );
  }
  gPublicList = await Downloads.getList(Downloads.PUBLIC);
});

const TESTCASES = [
  {
    name: "Null download arg",
    typeArg: "application/pdf",
    downloadProps: null,
    expected: /TypeError/,
  },
  {
    name: "Missing type arg",
    typeArg: undefined,
    downloadProps: {
      target: "download-test.pdf",
    },
    expected: /TypeError/,
  },
  {
    name: "Empty string type arg",
    typeArg: "",
    downloadProps: {
      target: "download-test.pdf",
    },
    expected: false,
  },
  {
    name: "download succeeded, file exists, unknown extension but contentType matches",
    typeArg: "application/pdf",
    downloadProps: {
      target: "download-test.xxunknown",
      contentType: "application/pdf",
    },
    expected: true,
  },
  {
    name: "download succeeded, file exists, contentType is generic and file extension maps to matching mime-type",
    typeArg: "application/pdf",
    downloadProps: {
      target: "download-test.pdf",
      contentType: "application/unknown",
    },
    expected: true,
  },
  {
    name: "download did not succeed",
    typeArg: "application/pdf",
    downloadProps: {
      target: "download-test.pdf",
      contentType: "application/pdf",
      succeeded: false,
    },
    expected: false,
  },
  {
    name: "file does not exist",
    typeArg: "application/pdf",
    downloadProps: {
      target: "download-test-missing.pdf",
      contentType: "application/pdf",
    },
    expected: false,
  },
  {
    name: "contentType is missing and file extension doesnt map to a known mime-type",
    typeArg: "application/pdf",
    downloadProps: {
      contentType: undefined,
      target: "download-test.xxunknown",
    },
    expected: false,
  },
];

for (let testData of TESTCASES) {
  let tmp = {
    async [testData.name]() {
      info("testing with: " + JSON.stringify(testData));
      await test_isFileOfType(testData);
    },
  };
  add_task(tmp[testData.name]);
}

/**
 * Sanity test the DownloadsCommon.isFileOfType method with test parameters
 */
async function test_isFileOfType({ name, typeArg, downloadProps, expected }) {
  let download, result;
  if (downloadProps) {
    let downloadData = {
      ...DOWNLOAD_TEMPLATE,
      ...downloadProps,
    };
    downloadData.target = TESTFILES[downloadData.target];
    Assert.ok(downloadData.target instanceof Ci.nsIFile, "target is a nsIFile");
    download = await Downloads.createDownload(downloadData);
    await gPublicList.add(download);
    await download.refresh();
  }

  if (typeof expected == "boolean") {
    result = await DownloadsCommon.isFileOfType(download, typeArg);
    Assert.equal(result, expected, "Expected result from call to isFileOfType");
  } else {
    Assert.throws(
      () => DownloadsCommon.isFileOfType(download, typeArg),
      expected,
      "isFileOfType should throw an exception if either the download object or mime-type arguments are falsey"
    );
  }
}
