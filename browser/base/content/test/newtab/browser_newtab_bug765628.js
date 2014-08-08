/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const BAD_DRAG_DATA = "javascript:alert('h4ck0rz');\nbad stuff";
const GOOD_DRAG_DATA = "http://example99.com/\nsite 99";

function runTests() {
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  sendDropEvent(0, BAD_DRAG_DATA);
  sendDropEvent(1, GOOD_DRAG_DATA);

  yield whenPagesUpdated();
  checkGrid("0,99p,1,2,3,4,5,6,7");
}

function sendDropEvent(aCellIndex, aDragData) {
  let ifaceReq = getContentWindow().QueryInterface(Ci.nsIInterfaceRequestor);
  let windowUtils = ifaceReq.getInterface(Ci.nsIDOMWindowUtils);

  let event = createDragEvent("drop", aDragData);
  windowUtils.dispatchDOMEventViaPresShell(getCell(aCellIndex).node, event, true);
}
