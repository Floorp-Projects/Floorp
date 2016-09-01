/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield* addNewTabPageTab();
  yield checkGrid("0,1,2,3,4,5,6,7,8");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    const BAD_DRAG_DATA = "javascript:alert('h4ck0rz');\nbad stuff";
    const GOOD_DRAG_DATA = "http://example99.com/\nsite 99";

    function sendDropEvent(aCellIndex, aDragData) {
      let dataTransfer = new content.DataTransfer("dragstart", false);
      dataTransfer.mozSetDataAt("text/x-moz-url", aDragData, 0);
      let event = content.document.createEvent("DragEvent");
      event.initDragEvent("drop", true, true, content, 0, 0, 0, 0, 0,
                          false, false, false, false, 0, null, dataTransfer);

      let target = content.gGrid.cells[aCellIndex].node;
      target.dispatchEvent(event);
    }

    sendDropEvent(0, BAD_DRAG_DATA);
    sendDropEvent(1, GOOD_DRAG_DATA);
  });

  yield whenPagesUpdated();
  yield* checkGrid("0,99p,1,2,3,4,5,6,7");
});
