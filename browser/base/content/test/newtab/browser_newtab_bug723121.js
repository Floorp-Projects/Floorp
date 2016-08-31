/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield* addNewTabPageTab();

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    let grid = content.gGrid;
    let cell = grid.cells[0];
    let site = cell.site.node;
    let link = site.querySelector(".newtab-link");

    function checkGridLocked(aLocked, aMessage) {
      Assert.equal(grid.node.hasAttribute("locked"), aLocked, aMessage);
    }

    function sendDragEvent(aEventType, aTarget) {
      let dataTransfer = new content.DataTransfer(aEventType, false);
      let event = content.document.createEvent("DragEvent");
      event.initDragEvent(aEventType, true, true, content, 0, 0, 0, 0, 0,
                          false, false, false, false, 0, null, dataTransfer);
      aTarget.dispatchEvent(event);
    }

    checkGridLocked(false, "grid is unlocked");

    sendDragEvent("dragstart", link);
    checkGridLocked(true, "grid is now locked");

    sendDragEvent("dragend", link);
    checkGridLocked(false, "grid isn't locked anymore");

    sendDragEvent("dragstart", cell.node);
    checkGridLocked(false, "grid isn't locked - dragstart was ignored");

    sendDragEvent("dragstart", site);
    checkGridLocked(false, "grid isn't locked - dragstart was ignored");
  });
});
