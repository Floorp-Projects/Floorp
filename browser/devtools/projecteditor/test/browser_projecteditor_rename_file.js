/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test file rename functionality

add_task(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  ok(true, "ProjectEditor has loaded");

  let root = [...projecteditor.project.allStores()][0].root;
  is(root.path, TEMP_PATH, "The root store is set to the correct temp path.");
  for (let child of root.children) {
    yield renameWithContextMenu(projecteditor, projecteditor.projectTree.getViewContainer(child));
  }
});

function openContextMenuOn(node) {
  EventUtils.synthesizeMouseAtCenter(
    node,
    {button: 2, type: "contextmenu"},
    node.ownerDocument.defaultView
  );
}

function renameWithContextMenu(projecteditor, container) {
  let defer = promise.defer();
  let popup = projecteditor.contextMenuPopup;
  let resource = container.resource;
  info ("Going to attempt renaming for: " + resource.path);

  onPopupShow(popup).then(function () {
    let renameCommand = popup.querySelector("[command=cmd-rename]");
    ok (renameCommand, "Rename command exists in popup");
    is (renameCommand.getAttribute("hidden"), "", "Rename command is visible");
    is (renameCommand.getAttribute("disabled"), "", "Rename command is enabled");

    projecteditor.project.on("refresh-complete", function refreshComplete() {
      projecteditor.project.off("refresh-complete", refreshComplete);
      OS.File.stat(resource.path + ".renamed").then(() => {
        ok (true, "File is renamed");
        defer.resolve();
      }, (ex) => {
        ok (false, "Failed to rename file");
        defer.resolve();
      });
    });

    renameCommand.click();
    popup.hidePopup();
    EventUtils.sendString(resource.basename + ".renamed", projecteditor.window);
    EventUtils.synthesizeKey("VK_RETURN", {}, projecteditor.window);
  });

  openContextMenuOn(container.label);
  return defer.promise;
}
