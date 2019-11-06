/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function runTests() {
  if (!SpecialPowers.getBoolPref("fission.sessionHistoryInParent")) {
    await setupAsync();
    let browser = gBrowser.selectedBrowser;
    // Now that we're set up, initialize our frame script.
    await checkListenersAsync("initial", "listeners initialized");

    // Check if all history listeners are always notified.
    info("# part 1");
    await whenPageShown(browser, () =>
      BrowserTestUtils.loadURI(browser, "http://www.example.com/")
    );
    await checkListenersAsync("newentry", "shistory has a new entry");
    ok(browser.canGoBack, "we can go back");

    await whenPageShown(browser, () => browser.goBack());
    await checkListenersAsync("gotoindex", "back to the first shentry");
    ok(browser.canGoForward, "we can go forward");

    await whenPageShown(browser, () => browser.goForward());
    await checkListenersAsync("gotoindex", "forward to the second shentry");

    await whenPageShown(browser, () => browser.reload());
    await checkListenersAsync("reload", "current shentry reloaded");

    await whenPageShown(browser, () => browser.gotoIndex(0));
    await checkListenersAsync("gotoindex", "back to the first index");

    // Check nsISHistory.notifyOnHistoryReload
    info("# part 2");
    ok(await notifyReloadAsync(), "reloading has not been canceled");
    await checkListenersAsync("reload", "saw the reload notification");

    // Let the first listener cancel the reload action.
    info("# part 3");
    await resetListenersAsync();
    await setListenerRetvalAsync(0, false);
    ok(!(await notifyReloadAsync()), "reloading has been canceled");
    await checkListenersAsync("reload", "saw the reload notification");

    // Let both listeners cancel the reload action.
    info("# part 4");
    await resetListenersAsync();
    await setListenerRetvalAsync(1, false);
    ok(!(await notifyReloadAsync()), "reloading has been canceled");
    await checkListenersAsync("reload", "saw the reload notification");

    // Let the second listener cancel the reload action.
    info("# part 5");
    await resetListenersAsync();
    await setListenerRetvalAsync(0, true);
    ok(!(await notifyReloadAsync()), "reloading has been canceled");
    await checkListenersAsync("reload", "saw the reload notification");

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

    function checkListenersAsync(aLast, aMessage) {
      return listenOnce("bug422543:getListenerStatus").then(
        listenerStatuses => {
          is(listenerStatuses[0], aLast, aMessage);
          is(listenerStatuses[1], aLast, aMessage);
        }
      );
    }

    function resetListenersAsync() {
      return listenOnce("bug422543:resetListeners");
    }

    function notifyReloadAsync() {
      return listenOnce("bug422543:notifyReload").then(({ rval }) => {
        return rval;
      });
    }

    function setListenerRetvalAsync(num, val) {
      return listenOnce("bug422543:setRetval", { num, val });
    }

    function setupAsync() {
      return BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        "http://mochi.test:8888"
      ).then(function(tab) {
        let browser = tab.linkedBrowser;
        registerCleanupFunction(async function() {
          await listenOnce("bug422543:cleanup");
          gBrowser.removeTab(tab);
        });

        browser.messageManager.loadFrameScript(
          getRootDirectory(gTestPath) + "file_bug422543_script.js",
          false
        );
      });
    }
    return;
  }

  await setup();
  let browser = gBrowser.selectedBrowser;
  // Now that we're set up, initialize our frame script.
  checkListeners("initial", "listeners initialized");

  // Check if all history listeners are always notified.
  info("# part 1");
  await whenPageShown(browser, () =>
    BrowserTestUtils.loadURI(browser, "http://www.example.com/")
  );
  checkListeners("newentry", "shistory has a new entry");
  ok(browser.canGoBack, "we can go back");

  await whenPageShown(browser, () => browser.goBack());
  checkListeners("gotoindex", "back to the first shentry");
  ok(browser.canGoForward, "we can go forward");

  await whenPageShown(browser, () => browser.goForward());
  checkListeners("gotoindex", "forward to the second shentry");

  await whenPageShown(browser, () => browser.reload());
  checkListeners("reload", "current shentry reloaded");

  await whenPageShown(browser, () => browser.gotoIndex(0));
  checkListeners("gotoindex", "back to the first index");

  // Check nsISHistory.notifyOnHistoryReload
  info("# part 2");
  ok(notifyReload(browser), "reloading has not been canceled");
  checkListeners("reload", "saw the reload notification");

  // Let the first listener cancel the reload action.
  info("# part 3");
  resetListeners();
  setListenerRetval(0, false);
  ok(!notifyReload(browser), "reloading has been canceled");
  checkListeners("reload", "saw the reload notification");

  // Let both listeners cancel the reload action.
  info("# part 4");
  resetListeners();
  setListenerRetval(1, false);
  ok(!notifyReload(browser), "reloading has been canceled");
  checkListeners("reload", "saw the reload notification");

  // Let the second listener cancel the reload action.
  info("# part 5");
  resetListeners();
  setListenerRetval(0, true);
  ok(!notifyReload(browser), "reloading has been canceled");
  checkListeners("reload", "saw the reload notification");
});

function SHistoryListener() {}

SHistoryListener.prototype = {
  retval: true,
  last: "initial",

  OnHistoryNewEntry(aNewURI) {
    this.last = "newentry";
  },

  OnHistoryGotoIndex() {
    this.last = "gotoindex";
  },

  OnHistoryPurge() {
    this.last = "purge";
  },

  OnHistoryReload() {
    this.last = "reload";
    return this.retval;
  },

  OnHistoryReplaceEntry() {},

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsISHistoryListener,
    Ci.nsISupportsWeakReference,
  ]),
};

let listeners = [new SHistoryListener(), new SHistoryListener()];

function checkListeners(aLast, aMessage) {
  is(listeners[0].last, aLast, aMessage);
  is(listeners[1].last, aLast, aMessage);
}

function resetListeners() {
  for (let listener of listeners) {
    listener.last = "initial";
  }
}

function notifyReload(browser) {
  return browser.browsingContext.sessionHistory.notifyOnHistoryReload();
}

function setListenerRetval(num, val) {
  listeners[num].retval = val;
}

function setup() {
  return BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888"
  ).then(function(tab) {
    let browser = tab.linkedBrowser;
    registerCleanupFunction(async function() {
      for (let listener of listeners) {
        browser.browsingContext.sessionHistory.removeSHistoryListener(listener);
      }
      gBrowser.removeTab(tab);
    });
    for (let listener of listeners) {
      browser.browsingContext.sessionHistory.addSHistoryListener(listener);
    }
  });
}

function whenPageShown(aBrowser, aNavigation) {
  let listener = ContentTask.spawn(aBrowser, null, function() {
    return new Promise(resolve => {
      addEventListener(
        "pageshow",
        function onLoad() {
          removeEventListener("pageshow", onLoad, true);
          resolve();
        },
        true
      );
    });
  });

  aNavigation();
  return listener;
}
