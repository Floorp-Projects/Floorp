add_task(function*() {
  let frame_script = () => {
    const { classes: Cc, interfaces: Ci } = Components;
    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"].
               getService(Ci.nsISyncMessageSender);

    sendAsyncMessage("Test:Frame:Async");
    cpmm.sendAsyncMessage("Test:Process:Async");
    cpmm.sendSyncMessage("Test:Process:Sync");
  };

  // This must be a page that opens in the main process
  let tab = gBrowser.addTab("about:robots");
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;
  let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"].
             getService(Ci.nsIMessageBroadcaster);
  yield new Promise(resolve => browser.addEventListener("load", resolve, true));

  let messages = [];
  let promise = new Promise(resolve => {
    let listener = message => {
      messages.push(message.name)
      if (messages.length == 3)
        resolve();
    };

    mm.addMessageListener("Test:Frame:Async", listener);
    ppmm.addMessageListener("Test:Process:Async", listener);
    ppmm.addMessageListener("Test:Process:Sync", listener);
  });

  mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);
  yield promise;

  gBrowser.removeTab(tab);

  is(messages[0], "Test:Frame:Async", "Expected async frame message");
  is(messages[1], "Test:Process:Async", "Expected async process message");
  is(messages[2], "Test:Process:Sync", "Expected sync process message");
});

add_task(function*() {
  let frame_script = () => {
    const { classes: Cc, interfaces: Ci } = Components;
    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"].
               getService(Ci.nsISyncMessageSender);

    cpmm.sendAsyncMessage("Test:Process:Async");
    sendAsyncMessage("Test:Frame:Async");
    sendSyncMessage("Test:Frame:Sync");
  };

  // This must be a page that opens in the main process
  let tab = gBrowser.addTab("about:robots");
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;
  let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"].
             getService(Ci.nsIMessageBroadcaster);
  yield new Promise(resolve => browser.addEventListener("load", resolve, true));

  let messages = [];
  let promise = new Promise(resolve => {
    let listener = message => {
      messages.push(message.name)
      if (messages.length == 3)
        resolve();
    };

    ppmm.addMessageListener("Test:Process:Async", listener);
    mm.addMessageListener("Test:Frame:Async", listener);
    mm.addMessageListener("Test:Frame:Sync", listener);
  });

  mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);
  yield promise;

  gBrowser.removeTab(tab);

  is(messages[0], "Test:Process:Async", "Expected async process message");
  is(messages[1], "Test:Frame:Async", "Expected async frame message");
  is(messages[2], "Test:Frame:Sync", "Expected sync frame message");
});
