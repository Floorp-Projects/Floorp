// This test verifies that the download panel opens when a
// download occurs but not when a user manually saves a page.

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

async function promiseDownloadFinished(list) {
  return new Promise(resolve => {
    list.addView({
      onDownloadChanged(download) {
        download.launchWhenSucceeded = false;
        if (download.succeeded || download.error) {
          list.removeView(this);
          resolve(download);
        }
      },
    });
  });
}

function openTestPage() {
  return BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `https://www.example.com/document-builder.sjs?html=
      <html><body>
        <a id='normallink' href='https://www.example.com'>Link1</a>
        <a id='downloadlink' href='https://www.example.com' download='file.txt'>Link2</a>
      </body</html>
    `
  );
}

add_task(async function download_saveas_file() {
  let tab = await openTestPage();

  for (let testname of ["save link", "save page"]) {
    if (testname == "save link") {
      let menu = document.getElementById("contentAreaContextMenu");
      let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      BrowserTestUtils.synthesizeMouse(
        "#normallink",
        5,
        5,
        { type: "contextmenu", button: 2 },
        gBrowser.selectedBrowser
      );
      await popupShown;
    }

    let list = await Downloads.getList(Downloads.PUBLIC);
    let downloadFinishedPromise = promiseDownloadFinished(list);

    let saveFile = Services.dirsvc.get("TmpD", Ci.nsIFile);
    saveFile.append("testsavedir");
    if (!saveFile.exists()) {
      saveFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    }

    await new Promise(resolve => {
      MockFilePicker.showCallback = function (fp) {
        saveFile.append("sample");
        MockFilePicker.setFiles([saveFile]);
        setTimeout(() => {
          resolve(fp.defaultString);
        }, 0);
        return Ci.nsIFilePicker.returnOK;
      };

      if (testname == "save link") {
        let menu = document.getElementById("contentAreaContextMenu");
        let menuitem = document.getElementById("context-savelink");
        menu.activateItem(menuitem);
      } else if (testname == "save page") {
        document.getElementById("Browser:SavePage").doCommand();
      }
    });

    await downloadFinishedPromise;
    is(
      DownloadsPanel.panel.state,
      "closed",
      "downloads panel closed after download link after " + testname
    );
  }

  await task_resetState();

  MockFilePicker.cleanup();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function download_link() {
  let tab = await openTestPage();

  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadFinishedPromise = promiseDownloadFinished(list);

  let panelOpenedPromise = promisePanelOpened();

  BrowserTestUtils.synthesizeMouse(
    "#downloadlink",
    5,
    5,
    {},
    gBrowser.selectedBrowser
  );

  let download = await downloadFinishedPromise;
  await panelOpenedPromise;

  is(
    DownloadsPanel.panel.state,
    "open",
    "downloads panel open after download link clicked"
  );

  DownloadsPanel.hidePanel();

  await task_resetState();

  BrowserTestUtils.removeTab(tab);

  try {
    await IOUtils.remove(download.target.path);
  } catch (ex) {}
});
