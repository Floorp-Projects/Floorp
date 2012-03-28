/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests ensure that the drop preview correctly arranges sites when
 * dragging them around.
 */
function runTests() {
  // the first three sites are pinned - make sure they're re-arranged correctly
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,5");

  yield addNewTabPageTab();
  checkGrid("0p,1p,2p,3,4,5p,6,7,8");

  let cw = getContentWindow();
  cw.gDrag._draggedSite = getCell(0).site;
  let sites = cw.gDropPreview.rearrange(getCell(4));
  cw.gDrag._draggedSite = null;

  checkGrid("3,1p,2p,4,0p,5p,6,7,8", sites);
}
