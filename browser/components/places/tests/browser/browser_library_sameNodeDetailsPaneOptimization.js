/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const sandbox = sinon.createSandbox();

/**
 * This checks an optimization in the Library code, that tries to not update
 * the details pane if the same node that is being edited is picked again.
 * That happens for example if the focus moves to the details pane and back
 * to the tree.
 */

add_task(async function () {
  let bm1 = await PlacesUtils.bookmarks.insert({
    url: "https://bookmark1.mozilla.org/",
    title: "Bookmark 1",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  let library = await promiseLibrary("UnfiledBookmarks");
  registerCleanupFunction(async () => {
    sandbox.restore();
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.remove(bm1);
  });

  let nameField = library.document.getElementById("editBMPanel_namePicker");
  let tree = library.ContentTree.view;
  tree.selectItems([bm1.guid]);
  Assert.equal(tree.selectedNode.title, "Bookmark 1");
  await synthesizeClickOnSelectedTreeCell(tree);
  Assert.equal(nameField.value, "Bookmark 1");

  let updateSpy = sandbox.spy(library.PlacesOrganizer, "updateDetailsPane");
  let uninitSpy = sandbox.spy(library.gEditItemOverlay, "uninitPanel");
  nameField.focus();
  await synthesizeClickOnSelectedTreeCell(tree);
  Assert.ok(updateSpy.calledOnce, "should try to update the details pane");
  Assert.ok(uninitSpy.notCalled, "should skip the update cause same node");
});
