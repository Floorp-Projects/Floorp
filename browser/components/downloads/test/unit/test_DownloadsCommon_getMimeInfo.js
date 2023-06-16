const DATA_PDF = atob(
  "JVBERi0xLjANCjEgMCBvYmo8PC9UeXBlL0NhdGFsb2cvUGFnZXMgMiAwIFI+PmVuZG9iaiAyIDAgb2JqPDwvVHlwZS9QYWdlcy9LaWRzWzMgMCBSXS9Db3VudCAxPj5lbmRvYmogMyAwIG9iajw8L1R5cGUvUGFnZS9NZWRpYUJveFswIDAgMyAzXT4+ZW5kb2JqDQp4cmVmDQowIDQNCjAwMDAwMDAwMDAgNjU1MzUgZg0KMDAwMDAwMDAxMCAwMDAwMCBuDQowMDAwMDAwMDUzIDAwMDAwIG4NCjAwMDAwMDAxMDIgMDAwMDAgbg0KdHJhaWxlcjw8L1NpemUgNC9Sb290IDEgMCBSPj4NCnN0YXJ0eHJlZg0KMTQ5DQolRU9G"
);

const DOWNLOAD_TEMPLATE = {
  source: {
    url: "https://example.com/download",
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
  "download-test.txt": "Text file contents\n",
  "download-test.pdf": DATA_PDF,
  "download-test.PDF": DATA_PDF,
  "download-test.xxunknown": "Unknown file contents\n",
  "download-test": "No extension file contents\n",
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
    name: "Check returned value is null when the download did not succeed",
    testFile: "download-test.txt",
    contentType: "text/plain",
    succeeded: false,
    expected: null,
  },
  {
    name: "Check correct mime-info is returned when download contentType is unambiguous",
    testFile: "download-test.txt",
    contentType: "text/plain",
    expected: {
      type: "text/plain",
    },
  },
  {
    name: "Returns correct mime-info from file extension when download contentType is missing",
    testFile: "download-test.pdf",
    contentType: undefined,
    expected: {
      type: "application/pdf",
    },
  },
  {
    name: "Returns correct mime-info from file extension case-insensitively",
    testFile: "download-test.PDF",
    contentType: undefined,
    expected: {
      type: "application/pdf",
    },
  },
  {
    name: "Returns null when contentType is missing and file extension is unknown",
    testFile: "download-test.xxunknown",
    contentType: undefined,
    expected: null,
  },
  {
    name: "Returns contentType when contentType is ambiguous and file extension is unknown",
    testFile: "download-test.xxunknown",
    contentType: "application/octet-stream",
    expected: {
      type: "application/octet-stream",
    },
  },
  {
    name: "Returns contentType when contentType is ambiguous and there is no file extension",
    testFile: "download-test",
    contentType: "application/octet-stream",
    expected: {
      type: "application/octet-stream",
    },
  },
  {
    name: "Returns null when there's no contentType and no file extension",
    testFile: "download-test",
    contentType: undefined,
    expected: null,
  },
];

// add tests for each of the generic mime-types we recognize,
// to ensure they prefer the associated mime-type of the target file extension
for (let type of [
  "application/octet-stream",
  "binary/octet-stream",
  "application/unknown",
]) {
  TESTCASES.push({
    name: `Returns correct mime-info from file extension when contentType is generic (${type})`,
    testFile: "download-test.pdf",
    contentType: type,
    expected: {
      type: "application/pdf",
    },
  });
}

for (let testData of TESTCASES) {
  let tmp = {
    async [testData.name]() {
      info("testing with: " + JSON.stringify(testData));
      await test_getMimeInfo_basic_function(testData);
    },
  };
  add_task(tmp[testData.name]);
}

/**
 * Sanity test the DownloadsCommon.getMimeInfo method with test parameters
 */
async function test_getMimeInfo_basic_function(testData) {
  let downloadData = {
    ...DOWNLOAD_TEMPLATE,
    source: "source" in testData ? testData.source : DOWNLOAD_TEMPLATE.source,
    succeeded:
      "succeeded" in testData
        ? testData.succeeded
        : DOWNLOAD_TEMPLATE.succeeded,
    target: TESTFILES[testData.testFile],
    contentType: testData.contentType,
  };
  Assert.ok(downloadData.target instanceof Ci.nsIFile, "target is a nsIFile");
  let download = await Downloads.createDownload(downloadData);
  await gPublicList.add(download);
  await download.refresh();

  Assert.ok(
    await IOUtils.exists(download.target.path),
    "The file should actually exist."
  );
  let result = await DownloadsCommon.getMimeInfo(download);
  if (testData.expected) {
    Assert.equal(
      result.type,
      testData.expected.type,
      "Got expected mimeInfo.type"
    );
  } else {
    Assert.equal(
      result,
      null,
      `Expected null, got object with type: ${result?.type}`
    );
  }
}
