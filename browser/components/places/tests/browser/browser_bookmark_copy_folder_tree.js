/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  let mainFolder = await PlacesUtils.bookmarks.insert({
    title: "mainFolder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
  });

  if (AppConstants.platform === "win") {
    if (Services.env.get("MOZ_AUTOMATION")) {
      // When running in CI, pre-emptively kill Windows Explorer, using system
      // from the standard library, since it sometimes holds the clipboard for
      // long periods, thereby breaking the test (bug 1921759).
      const { ctypes } = ChromeUtils.importESModule(
        "resource://gre/modules/ctypes.sys.mjs"
      );
      let libc = ctypes.open("ucrtbase.dll");
      let exec = libc.declare(
        "system",
        ctypes.default_abi,
        ctypes.int,
        ctypes.char.ptr
      );
      let rv = exec(
        '"powershell -command "&{&Stop-Process -ProcessName explorer}"'
      );
      libc.close();
      is(rv, 0, "Launched powershell to stop explorer.exe");
    } else {
      info(
        "Skipping terminating Windows Explorer since we are not running in automation"
      );
    }
  }

  await withSidebarTree("bookmarks", async function (tree) {
    const selectedNodeComparator = {
      equalTitle: itemNode => {
        Assert.equal(
          tree.selectedNode.title,
          itemNode.title,
          "Select expected title"
        );
      },
      equalNode: itemNode => {
        Assert.equal(
          tree.selectedNode.bookmarkGuid,
          itemNode.guid,
          "Selected the expected node"
        );
      },
      equalType: itemType => {
        Assert.equal(tree.selectedNode.type, itemType, "Correct type");
      },

      equalChildCount: childrenAmount => {
        Assert.equal(
          tree.selectedNode.childCount,
          childrenAmount,
          `${childrenAmount} children`
        );
      },
    };
    let urlType = Ci.nsINavHistoryResultNode.RESULT_TYPE_URI;

    info("Create tree of: folderA => subFolderA => 3 bookmarkItems");
    await PlacesUtils.bookmarks.insertTree({
      guid: mainFolder.guid,
      children: [
        {
          title: "FolderA",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          children: [
            {
              title: "subFolderA",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              children: [
                {
                  title: "firstBM",
                  url: "http://example.com/1",
                },
                {
                  title: "secondBM",
                  url: "http://example.com/2",
                },
                {
                  title: "thirdBM",
                  url: "http://example.com/3",
                },
              ],
            },
          ],
        },
      ],
    });

    info("Sanity check folderA, subFolderA, bookmarkItems");
    tree.selectItems([mainFolder.guid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalTitle(mainFolder);
    selectedNodeComparator.equalChildCount(1);

    let sourceFolder = tree.selectedNode.getChild(0);
    tree.selectNode(sourceFolder);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalTitle(sourceFolder);
    selectedNodeComparator.equalChildCount(1);

    let subSourceFolder = tree.selectedNode.getChild(0);
    tree.selectNode(subSourceFolder);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalTitle(subSourceFolder);
    selectedNodeComparator.equalChildCount(3);

    let bm_2 = tree.selectedNode.getChild(1);
    tree.selectNode(bm_2);
    selectedNodeComparator.equalTitle(bm_2);

    info(
      "Copy folder tree from sourceFolder (folderA, subFolderA, bookmarkItems)"
    );
    tree.selectNode(sourceFolder);
    await promiseClipboard(() => {
      tree.controller.copy();
    }, PlacesUtils.TYPE_X_MOZ_PLACE);

    tree.selectItems([mainFolder.guid]);

    info("Paste copy of folderA");
    await tree.controller.paste();

    info("Sanity check copy/paste operation - mainFolder has 2 children");
    tree.selectItems([mainFolder.guid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalChildCount(2);

    info("Sanity check copy of folderA");
    let copySourceFolder = tree.selectedNode.getChild(1);
    tree.selectNode(copySourceFolder);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalTitle(copySourceFolder);
    selectedNodeComparator.equalChildCount(1);

    info("Sanity check copy subFolderA");
    let copySubSourceFolder = tree.selectedNode.getChild(0);
    tree.selectNode(copySubSourceFolder);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalTitle(copySubSourceFolder);
    selectedNodeComparator.equalChildCount(3);

    info("Sanity check copy BookmarkItem");
    let copyBm_1 = tree.selectedNode.getChild(0);
    tree.selectNode(copyBm_1);
    selectedNodeComparator.equalTitle(copyBm_1);

    info("Undo copy operation");
    await PlacesTransactions.undo();
    tree.selectItems([mainFolder.guid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;

    info("Sanity check undo operation - mainFolder has 1 child");
    selectedNodeComparator.equalChildCount(1);

    info("Redo copy operation");
    await PlacesTransactions.redo();
    tree.selectItems([mainFolder.guid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;

    info("Sanity check redo operation - mainFolder has 2 children");
    selectedNodeComparator.equalChildCount(2);

    info("Sanity check copy of folderA");
    copySourceFolder = tree.selectedNode.getChild(1);
    tree.selectNode(copySourceFolder);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalChildCount(1);

    info("Sanity check copy subFolderA");
    copySubSourceFolder = tree.selectedNode.getChild(0);
    tree.selectNode(copySubSourceFolder);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    selectedNodeComparator.equalTitle(copySubSourceFolder);
    selectedNodeComparator.equalChildCount(3);

    info("Sanity check copy BookmarkItem");
    let copyBm_2 = tree.selectedNode.getChild(1);
    tree.selectNode(copyBm_2);
    selectedNodeComparator.equalTitle(copyBm_2);
    selectedNodeComparator.equalType(urlType);
  });
});
