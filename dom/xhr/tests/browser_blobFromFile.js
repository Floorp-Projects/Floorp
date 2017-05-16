let { classes: Cc, interfaces: Ci } = Components;

add_task(function* test() {
  yield SpecialPowers.pushPrefEnv(
    {set: [["browser.tabs.remote.separateFileUriProcess", true]]}
  );

  let file = Cc["@mozilla.org/file/directory_service;1"]
               .getService(Ci.nsIDirectoryService)
               .QueryInterface(Ci.nsIProperties)
               .get("ProfD", Ci.nsIFile);

  let fileHandler = Cc["@mozilla.org/network/io-service;1"]
                      .getService(Ci.nsIIOService)
                      .getProtocolHandler("file")
                      .QueryInterface(Ci.nsIFileProtocolHandler);

  let fileURL = fileHandler.getURLSpecFromFile(file);

  info("Opening url: " + fileURL);
  let tab = BrowserTestUtils.addTab(gBrowser, fileURL);

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);

  let blob = yield ContentTask.spawn(browser, null, function() {
    return new content.window.Promise(resolve => {
      let xhr = new content.window.XMLHttpRequest();
      xhr.responseType = "blob";
      xhr.open("GET", "prefs.js");
      xhr.send();
      xhr.onload = function() {
        resolve(xhr.response);
      }
    });
  });

  ok(blob instanceof File, "We have a file");

  file.append("prefs.js");
  is(blob.size, file.fileSize, "The size matches");
  is(blob.name, "prefs.js", "The name is correct");

  gBrowser.removeTab(tab);
});
