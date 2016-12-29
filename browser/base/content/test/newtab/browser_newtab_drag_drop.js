/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that dragging and dropping sites works as expected.
 * Sites contained in the grid need to shift around to indicate the result
 * of the drag-and-drop operation. If the grid is full and we're dragging
 * a new site into it another one gets pushed out.
 */
add_task(function* () {
  requestLongerTimeout(2);
  yield* addNewTabPageTab();

  // test a simple drag-and-drop scenario
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7,8");

  yield doDragEvent(0, 1);
  yield* checkGrid("1,0p,2,3,4,5,6,7,8");

  // drag a cell to its current cell and make sure it's not pinned afterwards
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7,8");

  yield doDragEvent(0, 0);
  yield* checkGrid("0,1,2,3,4,5,6,7,8");

  // ensure that pinned pages aren't moved if that's not necessary
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",1,2");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1p,2p,3,4,5,6,7,8");

  yield doDragEvent(0, 3);
  yield* checkGrid("3,1p,2p,0p,4,5,6,7,8");

  // pinned sites should always be moved around as blocks. if a pinned site is
  // moved around, neighboring pinned are affected as well
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1");

  yield* addNewTabPageTab();
  yield* checkGrid("0p,1p,2,3,4,5,6,7,8");

  yield doDragEvent(2, 0);
  yield* checkGrid("2p,0p,1p,3,4,5,6,7,8");

  // pinned sites should not be pushed out of the grid (unless there are only
  // pinned ones left on the grid)
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield doDragEvent(2, 5);
  yield* checkGrid("0,1,3,4,5,2p,6,7p,8p");

  // make sure that pinned sites are re-positioned correctly
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,5");

  yield* addNewTabPageTab();
  yield* checkGrid("0p,1p,2p,3,4,5p,6,7,8");

  yield doDragEvent(0, 4);
  yield* checkGrid("3,1p,2p,4,0p,5p,6,7,8");
});

function doDragEvent(sourceIndex, dropIndex) {
  return ContentTask.spawn(gBrowser.selectedBrowser,
                           { sourceIndex: sourceIndex, dropIndex: dropIndex }, function*(args) {
    let dataTransfer = new content.DataTransfer("dragstart", false);
    let event = content.document.createEvent("DragEvent");
    event.initDragEvent("dragstart", true, true, content, 0, 0, 0, 0, 0,
                        false, false, false, false, 0, null, dataTransfer);

    let target = content.gGrid.cells[args.sourceIndex].site.node;
    target.dispatchEvent(event);

    event = content.document.createEvent("DragEvent");
    event.initDragEvent("drop", true, true, content, 0, 0, 0, 0, 0,
                        false, false, false, false, 0, null, dataTransfer);

    target = content.gGrid.cells[args.dropIndex].node;
    target.dispatchEvent(event);
  });
}
