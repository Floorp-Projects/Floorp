/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Downloads",
  "resource://gre/modules/Downloads.jsm"
);

const OLD_NAMES = {
  [Downloads.PUBLIC]: "old-public",
  [Downloads.PRIVATE]: "old-private",
};
const RECENT_NAMES = {
  [Downloads.PUBLIC]: "recent-public",
  [Downloads.PRIVATE]: "recent-private",
};
const REFERENCE_DATE = new Date();
const OLD_DATE = new Date(Number(REFERENCE_DATE) - 10000);

async function downloadExists(list, path) {
  let listArray = await list.getAll();
  return listArray.some(i => i.target.path == path);
}

async function checkDownloads(
  expectOldExists = true,
  expectRecentExists = true
) {
  for (let listType of [Downloads.PUBLIC, Downloads.PRIVATE]) {
    let downloadsList = await Downloads.getList(listType);
    equal(
      await downloadExists(downloadsList, OLD_NAMES[listType]),
      expectOldExists,
      `Fake old download ${expectOldExists ? "was found" : "was removed"}.`
    );
    equal(
      await downloadExists(downloadsList, RECENT_NAMES[listType]),
      expectRecentExists,
      `Fake recent download ${
        expectRecentExists ? "was found" : "was removed"
      }.`
    );
  }
}

async function setupDownloads() {
  let downloadsList = await Downloads.getList(Downloads.ALL);
  await downloadsList.removeFinished();

  for (let listType of [Downloads.PUBLIC, Downloads.PRIVATE]) {
    downloadsList = await Downloads.getList(listType);
    let download = await Downloads.createDownload({
      source: {
        url: "https://bugzilla.mozilla.org/show_bug.cgi?id=1321303",
        isPrivate: listType == Downloads.PRIVATE,
      },
      target: OLD_NAMES[listType],
    });
    download.startTime = OLD_DATE;
    download.canceled = true;
    await downloadsList.add(download);

    download = await Downloads.createDownload({
      source: {
        url: "https://bugzilla.mozilla.org/show_bug.cgi?id=1321303",
        isPrivate: listType == Downloads.PRIVATE,
      },
      target: RECENT_NAMES[listType],
    });
    download.startTime = REFERENCE_DATE;
    download.canceled = true;
    await downloadsList.add(download);
  }

  // Confirm everything worked.
  downloadsList = await Downloads.getList(Downloads.ALL);
  equal((await downloadsList.getAll()).length, 4, "4 fake downloads added.");
  checkDownloads();
}

add_task(async function testDownloads() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeDownloads") {
        await browser.browsingData.removeDownloads(options);
      } else {
        await browser.browsingData.remove(options, { downloads: true });
      }
      browser.test.sendMessage("downloadsRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    // Clear downloads with no since value.
    await setupDownloads();
    extension.sendMessage(method, {});
    await extension.awaitMessage("downloadsRemoved");
    await checkDownloads(false, false);

    // Clear downloads with recent since value.
    await setupDownloads();
    extension.sendMessage(method, { since: REFERENCE_DATE });
    await extension.awaitMessage("downloadsRemoved");
    await checkDownloads(true, false);

    // Clear downloads with old since value.
    await setupDownloads();
    extension.sendMessage(method, { since: REFERENCE_DATE - 100000 });
    await extension.awaitMessage("downloadsRemoved");
    await checkDownloads(false, false);
  }

  await extension.startup();

  await testRemovalMethod("removeDownloads");
  await testRemovalMethod("remove");

  await extension.unload();
});
