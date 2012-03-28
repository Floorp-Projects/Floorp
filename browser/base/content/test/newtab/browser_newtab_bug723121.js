/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGridLocked(false, "grid is unlocked");

  let cell = getCell(0).node;
  let site = getCell(0).site.node;
  let link = site.querySelector(".newtab-link");

  sendDragEvent("dragstart", link);
  checkGridLocked(true, "grid is now locked");

  sendDragEvent("dragend", link);
  checkGridLocked(false, "grid isn't locked anymore");

  sendDragEvent("dragstart", cell);
  checkGridLocked(false, "grid isn't locked - dragstart was ignored");

  sendDragEvent("dragstart", site);
  checkGridLocked(false, "grid isn't locked - dragstart was ignored");
}

function checkGridLocked(aLocked, aMessage) {
  is(getGrid().node.hasAttribute("locked"), aLocked, aMessage);
}
