"use strict";

const BASE_ORIGIN = "https://example.com";
const URI = BASE_ORIGIN +
  "/browser/browser/components/contextualidentity/test/browser/saveLink.sjs";

const { Downloads } = Cu.import("resource://gre/modules/Downloads.jsm", {});

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

add_task(async function setup() {
  // make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true]
  ]});
});

add_task(async function test() {
  info("Let's create a temporary dir");
  let tempDir = createTemporarySaveDirectory();

  let downloadList = await Downloads.getList(Downloads.ALL);
  let all = await downloadList.getAll();
  is(all.length, 0, "No pending downloads!");

  info("Let's open a new window");
  let win = await BrowserTestUtils.openNewBrowserWindow();

  info("Opening tab with UCI: 1");
  let tab = BrowserTestUtils.addTab(win.gBrowser, URI + "?UCI=1", {userContextId: 1});

  // select tab and make sure its browser is focused
  win.gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  info("Waiting to load content");
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let path = await new Promise(resolve => {
    info("Register to handle popupshown");
    win.document.addEventListener("popupshown", event => {
      info("Context menu opened");

      let destFile = tempDir.clone();

      MockFilePicker.displayDirectory = tempDir;
      MockFilePicker.showCallback = fp => {
        info("MockFilePicker showCallback");
        let fileName = fp.defaultString;
        info("fileName: " + fileName);
        destFile.append(fileName);
        MockFilePicker.setFiles([destFile]);
        MockFilePicker.filterIndex = 1; // kSaveAsType_URL
        info("MockFilePicker showCallback done");
      };

      MockFilePicker.afterOpenCallback = () => {
        info("MockFilePicker afterOpenCallback");

        function resolveOrWait() {
          downloadList.getAll().then(downloads => {
            if (downloads.length) {
              is(downloads.length, 1, "We were expecting 1 download only.");
              ok(downloads[0].source.url.indexOf("saveLink.sjs") != -1, "The path is correct");

              downloads[0].whenSucceeded().then(() => {
                win.close();
                resolve(destFile.leafName);
              });
              return;
            }

            let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
            timer.initWithCallback(() => resolveOrWait, 500, Ci.nsITimer.TYPE_ONE_SHOT);
          });
        }

        resolveOrWait();
      };

      // Select "Save Link As" option from context menu
      let saveLinkCommand = win.document.getElementById("context-savelink");
      info("saveLinkCommand: " + saveLinkCommand);
      saveLinkCommand.doCommand();

      event.target.hidePopup();
      info("popup hidden");
    }, {once: true});

    BrowserTestUtils.synthesizeMouseAtCenter("#fff", {type: "contextmenu", button: 2},
                                             win.gBrowser.selectedBrowser);
    info("Right clicked!");
  });

  ok(path, "File downloaded correctly: " + path);

  let savedFile = tempDir.clone();
  savedFile.append(path);
  ok(savedFile.exists(), "We have the file");

  let content = readFile(savedFile);
  is(content, "cookie-present-UCI=1", "Correct file content: -" + content + "-");

  info("Removing the temporary directory");
  tempDir.remove(true);

  info("Closing the window");
  await BrowserTestUtils.closeWindow(win);
});

/* import-globals-from ../../../../../toolkit/content/tests/browser/common/mockTransfer.js */
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js", this);

function readFile(file) {
  let inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
  inputStream.init(file, -1, 0, 0);
  try {
    let scrInputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                           .createInstance(Ci.nsIScriptableInputStream);
    scrInputStream.init(inputStream);
    try {
      // Assume that the file is much shorter than 1 MiB.
      return scrInputStream.read(1048576);
    } finally {
      // Close the scriptable stream after reading, even if the operation
      // failed.
      scrInputStream.close();
    }
  } finally {
    // Close the stream after reading, if it is still open, even if the read
    // operation failed.
    inputStream.close();
  }
}

function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    info("create testsavedir!");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("return from createTempSaveDir: " + saveDir.path);
  return saveDir;
}
