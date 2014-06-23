// A hacky mechanism for catching and detecting that we're attempting
// to browse away is by setting the onbeforeunload event handler. We
// detect this dialog opening in the parent test script, and dismiss
// it.

function handleClickItem(aMessage) {
  let itemId = aMessage.data.details;
  content.console.log("Getting item with ID: " + itemId);
  let item = content.document.getElementById(itemId);
  item.click();
}

function handleAllowUnload(aMessage) {
  content.onbeforeunload = null;
  sendSyncMessage("TEST:allow-unload:done");
}

addMessageListener("TEST:click-item", handleClickItem);
addMessageListener("TEST:allow-unload", handleAllowUnload);