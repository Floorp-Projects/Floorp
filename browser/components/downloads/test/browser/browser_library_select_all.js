/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let gDownloadDir;

add_setup(async function () {
  await task_resetState();

  if (!gDownloadDir) {
    gDownloadDir = await setDownloadDir();
  }

  await task_addDownloads([
    {
      state: DownloadsCommon.DOWNLOAD_FINISHED,
      target: await createDownloadedFile(
        PathUtils.join(gDownloadDir, "downloaded_one.txt"),
        "Test file 1"
      ),
    },
    {
      state: DownloadsCommon.DOWNLOAD_FINISHED,
      target: await createDownloadedFile(
        PathUtils.join(gDownloadDir, "downloaded_two.txt"),
        "Test file 2"
      ),
    },
  ]);
  registerCleanupFunction(async function () {
    await task_resetState();
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_select_all() {
  let win = await openLibrary("Downloads");
  registerCleanupFunction(() => {
    win.close();
  });

  let listbox = win.document.getElementById("downloadsListBox");
  Assert.ok(listbox, "download list box present");
  listbox.focus();
  await TestUtils.waitForCondition(
    () => listbox.children.length == 2 && listbox.selectedItems.length == 1,
    "waiting for both items to be present with one selected"
  );
  info("Select all the downloads");
  win.goDoCommand("cmd_selectAll");
  Assert.equal(
    listbox.selectedItems.length,
    listbox.children.length,
    "All the items should be selected"
  );

  info("Search for a specific download");
  let searchBox = win.document.getElementById("searchFilter");
  searchBox.value = "_one";
  win.PlacesSearchBox.search(searchBox.value);
  await TestUtils.waitForCondition(() => {
    let visibleItems = Array.from(listbox.children).filter(c => !c.hidden);
    return (
      visibleItems.length == 1 &&
      visibleItems[0]._shell.download.target.path.includes("_one")
    );
  }, "Waiting for the search to complete");
  Assert.equal(
    listbox.selectedItems.length,
    0,
    "Check previous selection has been cleared by the search"
  );
  info("Select all the downloads");
  win.goDoCommand("cmd_selectAll");
  Assert.equal(listbox.children.length, 2, "Both items are present");
  Assert.equal(listbox.selectedItems.length, 1, "Only one item is selected");
  Assert.ok(!listbox.selectedItem.hidden, "The selected item is not hidden");
});
