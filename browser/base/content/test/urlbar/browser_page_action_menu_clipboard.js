"use strict";

const mockRemoteClients = [
  { id: "0", name: "foo", type: "mobile" },
  { id: "1", name: "bar", type: "desktop" },
  { id: "2", name: "baz", type: "mobile" },
];

add_task(async function copyURL() {
  // Open the panel.
  await promisePageActionPanelOpen();

  // Click Copy URL.
  let copyURLButton = document.getElementById("page-action-copy-url-button");
  let hiddenPromise = promisePageActionPanelHidden();
  EventUtils.synthesizeMouseAtCenter(copyURLButton, {});
  await hiddenPromise;

  // Check the clipboard.
  let transferable = Cc["@mozilla.org/widget/transferable;1"]
                       .createInstance(Ci.nsITransferable);
  transferable.init(null);
  let flavor = "text/unicode";
  transferable.addDataFlavor(flavor);
  Services.clipboard.getData(transferable, Services.clipboard.kGlobalClipboard);
  let strObj = {};
  transferable.getTransferData(flavor, strObj, {});
  Assert.ok(!!strObj.value);
  strObj.value.QueryInterface(Ci.nsISupportsString);
  Assert.equal(strObj.value.data, gBrowser.selectedBrowser.currentURI.spec);
});
