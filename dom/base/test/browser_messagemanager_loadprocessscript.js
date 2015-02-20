let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
           .getService(Ci.nsIMessageBroadcaster);
ppmm.QueryInterface(Ci.nsIProcessScriptLoader);

function processScript() {
  let cpmm = Components.classes["@mozilla.org/childprocessmessagemanager;1"]
           .getService(Components.interfaces.nsISyncMessageSender);
  if (cpmm !== this) {
    dump("Test failed: wrong global object\n");
    return;
  }

  this.cpmm = cpmm;

  addMessageListener("ProcessTest:Reply", function listener(msg) {
    removeMessageListener("ProcessTest:Reply", listener);
    sendAsyncMessage("ProcessTest:Finished");
  });
  sendSyncMessage("ProcessTest:Loaded");
}

function test() {
  waitForExplicitFinish();

  let replyCount = 0;

  function loadListener(msg) {
    replyCount++;
    msg.target.sendAsyncMessage("ProcessTest:Reply");
  }

  ppmm.addMessageListener("ProcessTest:Loaded", loadListener);
  ppmm.addMessageListener("ProcessTest:Finished", function finishListener(msg) {
    if (replyCount < ppmm.childCount) {
      return;
    }
    info("Got " + replyCount + " replies");
    ok(replyCount, "Got message reply");
    ppmm.removeMessageListener("ProcessTest:Loaded", loadListener);
    ppmm.removeMessageListener("ProcessTest:Finished", finishListener);
    finish();
  });
  ppmm.loadProcessScript("data:,(" + processScript.toString() + ")()", true);
}
