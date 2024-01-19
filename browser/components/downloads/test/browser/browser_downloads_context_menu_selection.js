/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that the context menu refers to the triggering item, even if the
 * selection was not set preemptively.
 */

async function createDownloadFiles() {
  let dir = await setDownloadDir();
  let downloads = [];
  downloads.push({
    state: DownloadsCommon.DOWNLOAD_FAILED,
    contentType: "text/plain",
    target: new FileUtils.File(PathUtils.join(dir, "does-not-exist.txt")),
  });
  downloads.push({
    state: DownloadsCommon.DOWNLOAD_FINISHED,
    contentType: "text/plain",
    target: await createDownloadedFile(PathUtils.join(dir, "file.txt"), "file"),
  });
  return downloads;
}

add_setup(async function setup() {
  await PlacesUtils.history.clear();
  await startServer();

  registerCleanupFunction(async function () {
    await task_resetState();
    await PlacesUtils.history.clear();
  });
});

add_task(async function test() {
  // remove download files, empty out collections
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  Assert.equal(downloadCount, 0, "There should be 0 downloads");
  await task_resetState();
  let downloads = await createDownloadFiles();
  await task_addDownloads(downloads);
  await task_openPanel();
  let downloadsListBox = document.getElementById("downloadsListBox");
  await TestUtils.waitForCondition(() => {
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == downloads.length;
  });

  // Note we're not doing anything to set the selectedItem here, exactly to
  // check the context menu doesn't depend on some selection prerequisite.

  let first = downloadsListBox.querySelector("richlistitem");
  let second = downloadsListBox.querySelector("richlistitem:nth-child(2)");

  info("Check first item");
  let firstDownload = DownloadsView.itemForElement(first).download;
  is(
    DownloadsCommon.stateOfDownload(firstDownload),
    DownloadsCommon.DOWNLOAD_FINISHED,
    "Download states match up"
  );
  // mousemove to the _other_ download, to ensure it doesn't confuse code.
  EventUtils.synthesizeMouse(second, -5, -5, { type: "mousemove" });
  await checkCommandsWithContextMenu(first, {
    downloadsCmd_show: true,
    cmd_delete: true,
  });

  info("Check second item");
  let secondDownload = DownloadsView.itemForElement(second).download;
  is(
    DownloadsCommon.stateOfDownload(secondDownload),
    DownloadsCommon.DOWNLOAD_FAILED,
    "Download states match up"
  );
  // mousemove to the _other_ download, to ensure it doesn't confuse code.
  EventUtils.synthesizeMouse(first, -5, -5, { type: "mousemove" });
  await checkCommandsWithContextMenu(second, {
    downloadsCmd_show: false,
    cmd_delete: true,
  });

  info("Check we don't open a context menu between items.");
  function listener() {
    Assert.ok(false, "Should not open a context menu");
  }
  document.addEventListener("popupshown", listener);
  let listRect = downloadsListBox.getBoundingClientRect();
  let firstRect = first.getBoundingClientRect();
  let secondRect = second.getBoundingClientRect();
  let x = parseInt(firstRect.width / 2);
  Assert.greater(
    secondRect.y - firstRect.y - firstRect.height,
    1,
    "There should be a gap of at least 1 px for this test"
  );
  let y = parseInt(firstRect.y - listRect.y + firstRect.height + 1);
  info(`Right click at (${x}, ${y})`);
  EventUtils.synthesizeMouse(downloadsListBox, x, y, {
    type: "contextmenu",
    button: 2,
  });
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 100));
  document.removeEventListener("popupshown", listener);

  let hiddenPromise = BrowserTestUtils.waitForEvent(
    DownloadsPanel.panel,
    "popuphidden"
  );
  DownloadsPanel.hidePanel();
  await hiddenPromise;
});

async function checkCommandsWithContextMenu(element, commands) {
  let contextMenu = await openContextMenu(element);
  for (let command in commands) {
    let enabled = commands[command];
    let commandStatus = enabled ? "enabled" : "disabled";
    info(`Checking command ${command} is ${commandStatus}`);

    let commandElt = contextMenu.querySelector(`[command="${command}"]`);
    Assert.equal(
      !BrowserTestUtils.isHidden(commandElt),
      enabled,
      `${command} should be ${enabled ? "visible" : "hidden"}`
    );

    Assert.strictEqual(
      DownloadsView.richListBox.selectedItem._shell.isCommandEnabled(command),
      enabled,
      `${command} should be ${commandStatus}`
    );
  }
  contextMenu.hidePopup();
}
