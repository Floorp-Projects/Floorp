"use strict";

function observeOnce(topic) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic) {
      if (topic == aTopic) {
        Services.obs.removeObserver(observer, topic);
        setTimeout(() => resolve(aSubject), 0);
      }
    }, topic);
  });
}

add_task(async function setPrefs() {
  await SpecialPowers.pushPrefEnv({"set": [["fission.oopif.attribute", true],
                                           ["dom.ipc.processCount", 10000]]});
});

add_task(async function runTest() {
  info(`chrome, parent`);
  let chromeBC = window.docShell.browsingContext;
  ok(chromeBC.currentWindowGlobal, "Should have a current WindowGlobal");
  is(chromeBC.embedderWindowGlobal, null, "chrome has no embedder global");
  is(chromeBC.embedderElement, null, "chrome has no embedder element");
  is(chromeBC.parent, null, "chrome has no parent");

  // Open a new tab, and check that basic frames work out.
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser});

  info(`root, parent`);
  let rootBC = tab.linkedBrowser.browsingContext;
  ok(rootBC.currentWindowGlobal, "[parent] root has a window global");
  is(rootBC.embedderWindowGlobal, chromeBC.currentWindowGlobal, "[parent] root has chrome as embedder global");
  is(rootBC.embedderElement, tab.linkedBrowser, "[parent] root has browser as embedder element");
  is(rootBC.parent, null, "[parent] root has no parent");

  // Test with an in-process frame
  let frameId = await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    info(`root, child`);
    let rootBC = content.docShell.browsingContext;
    is(rootBC.embedderElement, null, "[child] root has no embedder");
    is(rootBC.parent, null, "[child] root has no parent");

    info(`frame, child`);
    let iframe = content.document.createElement("iframe");
    content.document.body.appendChild(iframe);

    let frameBC = iframe.contentWindow.docShell.browsingContext;
    is(frameBC.embedderElement, iframe, "[child] frame embedded within iframe");
    is(frameBC.parent, rootBC, "[child] frame has root as parent");

    return frameBC.id;
  });

  info(`frame, parent`);
  let frameBC = BrowsingContext.get(frameId);
  ok(frameBC.currentWindowGlobal, "[parent] frame has a window global");
  is(frameBC.embedderWindowGlobal, rootBC.currentWindowGlobal, "[parent] frame has root as embedder global");
  is(frameBC.embedderElement, null, "[parent] frame has no embedder element");
  is(frameBC.parent, rootBC, "[parent] frame has root as parent");

  // Test with an out-of-process iframe.

  info(`registering oop windowglobal promise`);
  // Unfortunately we currently don't create a WindowGlobal for the initial
  // about:blank synchronously, so we need to set up a promise to wait for its
  // creation.
  let oopWindowGlobalPromise = new Promise(resolve => {
    const TOPIC = "window-global-created";
    Services.obs.addObserver(function observer(subject) {
      info(`Saw ${TOPIC}: ${subject}`);
      if (subject.browsingContext.parent == rootBC) {
        info(`Resolving with ${subject}`);
        Services.obs.removeObserver(observer, TOPIC);
        resolve(subject);
      }
    }, TOPIC);
  });

  let oopID = await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    info(`creating oop iframe`);
    let oop = content.document.createElement("iframe");
    oop.setAttribute("fission", "true");
    content.document.body.appendChild(oop);

    info(`oop frame, child`);
    let oopBC = oop.frameLoader.browsingContext;
    is(oopBC.embedderElement, oop, "[child] oop frame embedded within iframe");
    is(oopBC.parent, content.docShell.browsingContext, "[child] frame has root as parent");

    return oopBC.id;
  });

  info(`oop frame, parent`);
  let oopBC = BrowsingContext.get(oopID);
  is(oopBC.embedderWindowGlobal, rootBC.currentWindowGlobal, "[parent] oop frame has root as embedder global");
  is(oopBC.embedderElement, null, "[parent] oop frame has no embedder element");
  is(oopBC.parent, rootBC, "[parent] oop frame has root as parent");

  info(`waiting for oop window global`);
  let oopWindowGlobal = await oopWindowGlobalPromise;
  is(oopBC.currentWindowGlobal, oopWindowGlobal, "[parent] oop frame has a window global");

  // Open a new window, and adopt |tab| into it.

  let newWindow = await BrowserTestUtils.openNewBrowserWindow();

  info(`new chrome, parent`);
  let newChromeBC = newWindow.docShell.browsingContext;
  ok(newChromeBC.currentWindowGlobal, "Should have a current WindowGlobal");
  is(newChromeBC.embedderWindowGlobal, null, "new chrome has no embedder global");
  is(newChromeBC.embedderElement, null, "new chrome has no embedder element");
  is(newChromeBC.parent, null, "new chrome has no parent");

  isnot(newChromeBC, chromeBC, "different chrome browsing context");

  info(`adopting tab`);
  let newTab = newWindow.gBrowser.adoptTab(tab);

  is(newTab.linkedBrowser.browsingContext, rootBC, "[parent] root browsing context survived");
  is(rootBC.embedderWindowGlobal, newChromeBC.currentWindowGlobal, "[parent] embedder window global updated");
  is(rootBC.embedderElement, newTab.linkedBrowser, "[parent] embedder element updated");
  is(rootBC.parent, null, "[parent] root has no parent");

  info(`closing window`);
  await BrowserTestUtils.closeWindow(newWindow);
});
