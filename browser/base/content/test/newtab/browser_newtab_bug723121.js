/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGridLocked(false, "grid is unlocked");

  let cell = cells[0].node;
  let site = cells[0].site.node;

  sendDragEvent(site, "dragstart");
  checkGridLocked(true, "grid is now locked");

  sendDragEvent(site, "dragend");
  checkGridLocked(false, "grid isn't locked anymore");

  sendDragEvent(cell, "dragstart");
  checkGridLocked(false, "grid isn't locked - dragstart was ignored");
}

function checkGridLocked(aLocked, aMessage) {
  is(cw.gGrid.node.hasAttribute("locked"), aLocked, aMessage);
}

function sendDragEvent(aNode, aType) {
  let ifaceReq = cw.QueryInterface(Ci.nsIInterfaceRequestor);
  let windowUtils = ifaceReq.getInterface(Ci.nsIDOMWindowUtils);

  let dataTransfer = {
    mozUserCancelled: false,
    setData: function () null,
    setDragImage: function () null,
    getData: function () "about:blank"
  };

  let event = cw.document.createEvent("DragEvents");
  event.initDragEvent(aType, true, true, cw, 0, 0, 0, 0, 0,
                      false, false, false, false, 0, null, dataTransfer);

  windowUtils.dispatchDOMEventViaPresShell(aNode, event, true);
}
