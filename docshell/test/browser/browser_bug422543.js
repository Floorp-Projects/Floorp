/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ACTOR = "Bug422543";

let getActor = browser => {
  return browser.browsingContext.currentWindowGlobal.getActor(ACTOR);
};

add_task(async function runTests() {
  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    await setupAsync();
    let browser = gBrowser.selectedBrowser;
    // Now that we're set up, initialize our frame script.
    await checkListenersAsync("initial", "listeners initialized");

    // Check if all history listeners are always notified.
    info("# part 1");
    await whenPageShown(browser, () =>
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      BrowserTestUtils.startLoadingURIString(browser, "http://www.example.com/")
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

    function sendQuery(message, arg = {}) {
      return getActor(gBrowser.selectedBrowser).sendQuery(message, arg);
    }

    function checkListenersAsync(aLast, aMessage) {
      return sendQuery("getListenerStatus").then(listenerStatuses => {
        is(listenerStatuses[0], aLast, aMessage);
        is(listenerStatuses[1], aLast, aMessage);
      });
    }

    function resetListenersAsync() {
      return sendQuery("resetListeners");
    }

    function notifyReloadAsync() {
      return sendQuery("notifyReload").then(({ rval }) => {
        return rval;
      });
    }

    function setListenerRetvalAsync(num, val) {
      return sendQuery("setRetval", { num, val });
    }

    async function setupAsync() {
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        "http://mochi.test:8888"
      );

      let base = getRootDirectory(gTestPath).slice(0, -1);
      ChromeUtils.registerWindowActor(ACTOR, {
        child: {
          esModuleURI: `${base}/Bug422543Child.sys.mjs`,
        },
      });

      registerCleanupFunction(async () => {
        await sendQuery("cleanup");
        gBrowser.removeTab(tab);

        ChromeUtils.unregisterWindowActor(ACTOR);
      });

      await sendQuery("init");
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
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    BrowserTestUtils.startLoadingURIString(browser, "http://www.example.com/")
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

class SHistoryListener {
  constructor() {
    this.retval = true;
    this.last = "initial";
  }

  OnHistoryNewEntry(aNewURI) {
    this.last = "newentry";
  }

  OnHistoryGotoIndex() {
    this.last = "gotoindex";
  }

  OnHistoryPurge() {
    this.last = "purge";
  }

  OnHistoryReload() {
    this.last = "reload";
    return this.retval;
  }

  OnHistoryReplaceEntry() {}
}
SHistoryListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsISHistoryListener",
  "nsISupportsWeakReference",
]);

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

async function setup() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888"
  );

  let browser = tab.linkedBrowser;
  registerCleanupFunction(async function () {
    for (let listener of listeners) {
      browser.browsingContext.sessionHistory.removeSHistoryListener(listener);
    }
    gBrowser.removeTab(tab);
  });
  for (let listener of listeners) {
    browser.browsingContext.sessionHistory.addSHistoryListener(listener);
  }
}

function whenPageShown(aBrowser, aNavigation) {
  let promise = new Promise(resolve => {
    let unregister = BrowserTestUtils.addContentEventListener(
      aBrowser,
      "pageshow",
      () => {
        unregister();
        resolve();
      },
      { capture: true }
    );
  });

  aNavigation();
  return promise;
}
