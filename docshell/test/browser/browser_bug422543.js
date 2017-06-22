/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function runTests() {
  await setup();
  let browser = gBrowser.selectedBrowser;
  // Now that we're set up, initialize our frame script.
  await checkListeners("initial", "listeners initialized");

  // Check if all history listeners are always notified.
  info("# part 1");
  await whenPageShown(browser, () => browser.loadURI("http://www.example.com/"));
  await checkListeners("newentry", "shistory has a new entry");
  ok(browser.canGoBack, "we can go back");

  await whenPageShown(browser, () => browser.goBack());
  await checkListeners("goback", "back to the first shentry");
  ok(browser.canGoForward, "we can go forward");

  await whenPageShown(browser, () => browser.goForward());
  await checkListeners("goforward", "forward to the second shentry");

  await whenPageShown(browser, () => browser.reload());
  await checkListeners("reload", "current shentry reloaded");

  await whenPageShown(browser, () => browser.gotoIndex(0));
  await checkListeners("gotoindex", "back to the first index");

  // Check nsISHistoryInternal.notifyOnHistoryReload
  info("# part 2");
  ok((await notifyReload()), "reloading has not been canceled");
  await checkListeners("reload", "saw the reload notification");

  // Let the first listener cancel the reload action.
  info("# part 3");
  await resetListeners();
  await setListenerRetval(0, false);
  ok(!(await notifyReload()), "reloading has been canceled");
  await checkListeners("reload", "saw the reload notification");

  // Let both listeners cancel the reload action.
  info("# part 4");
  await resetListeners();
  await setListenerRetval(1, false);
  ok(!(await notifyReload()), "reloading has been canceled");
  await checkListeners("reload", "saw the reload notification");

  // Let the second listener cancel the reload action.
  info("# part 5");
  await resetListeners();
  await setListenerRetval(0, true);
  ok(!(await notifyReload()), "reloading has been canceled");
  await checkListeners("reload", "saw the reload notification");
});

function listenOnce(message, arg = {}) {
  return new Promise(resolve => {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.addMessageListener(message + ":return", function listener(msg) {
      mm.removeMessageListener(message + ":return", listener);
      resolve(msg.data);
    });

    mm.sendAsyncMessage(message, arg);
  });
}

function checkListeners(aLast, aMessage) {
  return listenOnce("bug422543:getListenerStatus").then((listenerStatuses) => {
    is(listenerStatuses[0], aLast, aMessage);
    is(listenerStatuses[1], aLast, aMessage);
  });
}

function resetListeners() {
  return listenOnce("bug422543:resetListeners");
}

function notifyReload() {
  return listenOnce("bug422543:notifyReload").then(({ rval }) => {
    return rval;
  });
}

function setListenerRetval(num, val) {
  return listenOnce("bug422543:setRetval", { num, val });
}

function setup() {
  return BrowserTestUtils.openNewForegroundTab(gBrowser,
                                               "http://mochi.test:8888")
                         .then(function (tab) {
    let browser = tab.linkedBrowser;
    registerCleanupFunction(async function() {
      await listenOnce("bug422543:cleanup");
      gBrowser.removeTab(tab);
    });

    browser.messageManager
           .loadFrameScript(getRootDirectory(gTestPath) + "file_bug422543_script.js", false);
  });
}

function whenPageShown(aBrowser, aNavigation) {
  let listener = ContentTask.spawn(aBrowser, null, function () {
    return new Promise(resolve => {
      addEventListener("pageshow", function onLoad() {
        removeEventListener("pageshow", onLoad, true);
        resolve();
      }, true);
    });
  });

  aNavigation();
  return listener;
}
