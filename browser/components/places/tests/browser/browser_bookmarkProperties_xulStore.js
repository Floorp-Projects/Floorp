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

  await withSidebarTree("bookmarks", async function (tree) {
    await PlacesUtils.bookmarks.insertTree({
      guid: mainFolder.guid,
      children: [
        {
          title: "firstFolder",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
        },
        {
          title: "secondFolder",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
        },
      ],
    });
    tree.selectItems([mainFolder.guid]);
    PlacesUtils.asContainer(tree.selectedNode).containerOpen = true;
    let firstFolder = tree.selectedNode.getChild(0);

    tree.selectNode(firstFolder);
    info("Synthesize click on selected node to open it.");
    synthesizeClickOnSelectedTreeCell(tree);
    info(`Get the hashed uri starts with "place:" and hash key&value pairs.`);
    let hashedKey = PlacesUIUtils.obfuscateUrlForXulStore(firstFolder.uri);

    let docUrl = "chrome://browser/content/places/bookmarksSidebar.xhtml";

    let value = Services.xulStore.getValue(docUrl, hashedKey, "open");

    Assert.ok(hashedKey.startsWith("place:"), "Sanity check the hashed key");
    Assert.equal(value, "true", "Check the expected xulstore value");
  });
});
