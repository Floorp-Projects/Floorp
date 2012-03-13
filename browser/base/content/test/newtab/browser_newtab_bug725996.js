/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  let cell = cells[0].node;

  sendDropEvent(cell, "about:blank#99\nblank");
  is(NewTabUtils.pinnedLinks.links[0].url, "about:blank#99",
     "first cell is pinned and contains the dropped site");

  yield whenPagesUpdated();
  checkGrid("99p,0,1,2,3,4,5,6,7");

  sendDropEvent(cell, "");
  is(NewTabUtils.pinnedLinks.links[0].url, "about:blank#99",
     "first cell is still pinned with the site we dropped before");
}

function sendDropEvent(aNode, aData) {
  let ifaceReq = cw.QueryInterface(Ci.nsIInterfaceRequestor);
  let windowUtils = ifaceReq.getInterface(Ci.nsIDOMWindowUtils);

  let dataTransfer = {
    mozUserCancelled: false,
    setData: function () null,
    setDragImage: function () null,
    getData: function () aData,

    types: {
      contains: function (aType) aType == "text/x-moz-url"
    },

    mozGetDataAt: function (aType, aIndex) {
      if (aIndex || aType != "text/x-moz-url")
        return null;

      return aData;
    },
  };

  let event = cw.document.createEvent("DragEvents");
  event.initDragEvent("drop", true, true, cw, 0, 0, 0, 0, 0,
                      false, false, false, false, 0, null, dataTransfer);

  windowUtils.dispatchDOMEventViaPresShell(aNode, event, true);
}
