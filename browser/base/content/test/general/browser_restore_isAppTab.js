/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

const DUMMY = "http://example.com/browser/browser/base/content/test/general/dummy_page.html";

function getMinidumpDirectory() {
  let dir = Services.dirsvc.get('ProfD', Ci.nsIFile);
  dir.append("minidumps");
  return dir;
}

// This observer is needed so we can clean up all evidence of the crash so
// the testrunner thinks things are peachy.
var CrashObserver = {
  observe: function(subject, topic, data) {
    is(topic, 'ipc:content-shutdown', 'Received correct observer topic.');
    ok(subject instanceof Ci.nsIPropertyBag2,
       'Subject implements nsIPropertyBag2.');
    // we might see this called as the process terminates due to previous tests.
    // We are only looking for "abnormal" exits...
    if (!subject.hasKey("abnormal")) {
      info("This is a normal termination and isn't the one we are looking for...");
      return;
    }

    let dumpID;
    if ('nsICrashReporter' in Ci) {
      dumpID = subject.getPropertyAsAString('dumpID');
      ok(dumpID, "dumpID is present and not an empty string");
    }

    if (dumpID) {
      let minidumpDirectory = getMinidumpDirectory();
      let file = minidumpDirectory.clone();
      file.append(dumpID + '.dmp');
      file.remove(true);
      file = minidumpDirectory.clone();
      file.append(dumpID + '.extra');
      file.remove(true);
    }
  }
}
Services.obs.addObserver(CrashObserver, 'ipc:content-shutdown', false);

registerCleanupFunction(() => {
  Services.obs.removeObserver(CrashObserver, 'ipc:content-shutdown');
});

function frameScript() {
  addMessageListener("Test:GetIsAppTab", function() {
    sendAsyncMessage("Test:IsAppTab", { isAppTab: docShell.isAppTab });
  });

  addMessageListener("Test:Crash", function() {
    privateNoteIntentionalCrash();
    Components.utils.import("resource://gre/modules/ctypes.jsm");
    let zero = new ctypes.intptr_t(8);
    let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
    badptr.contents
  });
}

function loadFrameScript(browser) {
  browser.messageManager.loadFrameScript("data:,(" + frameScript.toString() + ")();", true);
}

function isBrowserAppTab(browser) {
  return new Promise(resolve => {
    function listener({ data }) {
      browser.messageManager.removeMessageListener("Test:IsAppTab", listener);
      resolve(data.isAppTab);
    }
    // It looks like same-process messages may be reordered by the message
    // manager, so we need to wait one tick before sending the message.
    executeSoon(function() {
      browser.messageManager.addMessageListener("Test:IsAppTab", listener);
      browser.messageManager.sendAsyncMessage("Test:GetIsAppTab");
    });
  });
}

// Restarts the child process by crashing it then reloading the tab
var restart = Task.async(function*(browser) {
  // If the tab isn't remote this would crash the main process so skip it
  if (!browser.isRemoteBrowser)
    return;

  // Make sure the main process has all of the current tab state before crashing
  yield TabStateFlusher.flush(browser);

  browser.messageManager.sendAsyncMessage("Test:Crash");
  yield promiseWaitForEvent(browser, "AboutTabCrashedLoad", false, true);

  let tab = gBrowser.getTabForBrowser(browser);
  SessionStore.reviveCrashedTab(tab);

  yield promiseTabLoaded(tab);
});

add_task(function* navigate() {
  let tab = gBrowser.addTab("about:robots");
  let browser = tab.linkedBrowser;
  gBrowser.selectedTab = tab;
  yield waitForDocLoadComplete();
  loadFrameScript(browser);
  let isAppTab = yield isBrowserAppTab(browser);
  ok(!isAppTab, "Docshell shouldn't think it is an app tab");

  gBrowser.pinTab(tab);
  isAppTab = yield isBrowserAppTab(browser);
  ok(isAppTab, "Docshell should think it is an app tab");

  gBrowser.loadURI(DUMMY);
  yield waitForDocLoadComplete();
  loadFrameScript(browser);
  isAppTab = yield isBrowserAppTab(browser);
  ok(isAppTab, "Docshell should think it is an app tab");

  gBrowser.unpinTab(tab);
  isAppTab = yield isBrowserAppTab(browser);
  ok(!isAppTab, "Docshell shouldn't think it is an app tab");

  gBrowser.pinTab(tab);
  isAppTab = yield isBrowserAppTab(browser);
  ok(isAppTab, "Docshell should think it is an app tab");

  gBrowser.loadURI("about:robots");
  yield waitForDocLoadComplete();
  loadFrameScript(browser);
  isAppTab = yield isBrowserAppTab(browser);
  ok(isAppTab, "Docshell should think it is an app tab");

  gBrowser.removeCurrentTab();
});

add_task(function* crash() {
  if (!gMultiProcessBrowser || !("nsICrashReporter" in Ci))
    return;

  let tab = gBrowser.addTab(DUMMY);
  let browser = tab.linkedBrowser;
  gBrowser.selectedTab = tab;
  yield waitForDocLoadComplete();
  loadFrameScript(browser);
  let isAppTab = yield isBrowserAppTab(browser);
  ok(!isAppTab, "Docshell shouldn't think it is an app tab");

  gBrowser.pinTab(tab);
  isAppTab = yield isBrowserAppTab(browser);
  ok(isAppTab, "Docshell should think it is an app tab");

  yield restart(browser);
  loadFrameScript(browser);
  isAppTab = yield isBrowserAppTab(browser);
  ok(isAppTab, "Docshell should think it is an app tab");

  gBrowser.removeCurrentTab();
});
