add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.remote.separateFileUriProcess", true]],
  });

  let fileData = "";
  for (var i = 0; i < 100; ++i) {
    fileData += "hello world!";
  }

  let file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("ProfD", Ci.nsIFile);
  file.append("file.txt");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  let outStream = Cc[
    "@mozilla.org/network/file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);
  outStream.init(
    file,
    0x02 | 0x08 | 0x20, // write, create, truncate
    0o666,
    0
  );
  outStream.write(fileData, fileData.length);
  outStream.close();

  let fileHandler = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService)
    .getProtocolHandler("file")
    .QueryInterface(Ci.nsIFileProtocolHandler);

  let fileURL = fileHandler.getURLSpecFromActualFile(file);

  info("Opening url: " + fileURL);
  let tab = BrowserTestUtils.addTab(gBrowser, fileURL);

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let blob = await SpecialPowers.spawn(browser, [file.leafName], function(
    fileName
  ) {
    return new content.window.Promise(resolve => {
      content.window
        .fetch(fileName)
        .then(r => r.blob())
        .then(blob => resolve(blob));
    });
  });

  ok(blob instanceof File, "We have a file");

  is(blob.size, file.fileSize, "The size matches");
  is(blob.name, file.leafName, "The name is correct");

  file.remove(false);

  gBrowser.removeTab(tab);
});
