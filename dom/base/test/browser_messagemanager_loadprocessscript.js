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
let processScriptURL = "data:,(" + processScript.toString() + ")()";

function initTestScript() {
  let init = initialProcessData;
  if (init.test123 != "hello") {
    dump("Initial data incorrect\n");
    return;
  }

  sendAsyncMessage("ProcessTest:InitGood", init.test456.get("hi"));
}
let initTestScriptURL = "data:,(" + initTestScript.toString() + ")()";

let checkProcess = Task.async(function*(mm) {
  let { target } = yield promiseMessage(mm, "ProcessTest:Loaded");
  target.sendAsyncMessage("ProcessTest:Reply");
  yield promiseMessage(target, "ProcessTest:Finished");
  ok(true, "Saw process finished");
});

function promiseMessage(messageManager, message) {
  return new Promise(resolve => {
    let listener = (msg) => {
      messageManager.removeMessageListener(message, listener);
      resolve(msg);
    };

    messageManager.addMessageListener(message, listener);
  })
}

// Test that loading a process script loads in all existing processes
add_task(function*() {
  let checks = [];
  for (let i = 0; i < ppmm.childCount; i++)
    checks.push(checkProcess(ppmm.getChildAt(i)));

  ppmm.loadProcessScript(processScriptURL, false);
  yield Promise.all(checks);
});

// Test that loading a process script loads in new processes
add_task(function*() {
  // This test is only relevant in e10s
  if (!gMultiProcessBrowser)
    return;

  is(ppmm.childCount, 2, "Should be two processes at this point");

  // Load something in the main process
  gBrowser.selectedBrowser.loadURI("about:robots");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let init = ppmm.initialProcessData;
  init.test123 = "hello";
  init.test456 = new Map();
  init.test456.set("hi", "bye");

  // With no remote frames left we should be down to one process.
  // However, stuff like remote thumbnails can cause a content
  // process to exist nonetheless. This should be rare, though,
  // so the test is useful most of the time.
  if (ppmm.childCount == 1) {
    let mainMM = ppmm.getChildAt(0);

    let check = checkProcess(ppmm);
    ppmm.loadProcessScript(processScriptURL, true);

    // The main process should respond
    yield check;

    check = checkProcess(ppmm);
    // Reset the default browser to start a new child process
    gBrowser.updateBrowserRemoteness(gBrowser.selectedBrowser, true);
    gBrowser.selectedBrowser.loadURI("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    is(ppmm.childCount, 2, "Should be back to two processes at this point");

    // The new process should have responded
    yield check;

    ppmm.removeDelayedProcessScript(processScriptURL);

    let childMM;
    childMM = ppmm.getChildAt(0) == mainMM ? ppmm.getChildAt(1) : ppmm.getChildAt(0);

    childMM.loadProcessScript(initTestScriptURL, false);
    let msg = yield promiseMessage(childMM, "ProcessTest:InitGood");
    is(msg.data, "bye", "initial process data was correct");
  } else {
    info("Unable to finish test entirely");
  }
});
