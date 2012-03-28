/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  let cell = getCell(0).node;

  sendDragEvent("drop", cell, "about:blank#99\nblank");
  is(NewTabUtils.pinnedLinks.links[0].url, "about:blank#99",
     "first cell is pinned and contains the dropped site");

  yield whenPagesUpdated();
  checkGrid("99p,0,1,2,3,4,5,6,7");

  sendDragEvent("drop", cell, "");
  is(NewTabUtils.pinnedLinks.links[0].url, "about:blank#99",
     "first cell is still pinned with the site we dropped before");
}
