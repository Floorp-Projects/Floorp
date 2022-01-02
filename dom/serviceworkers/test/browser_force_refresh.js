/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var gTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

async function refresh() {
  EventUtils.synthesizeKey("R", { accelKey: true });
}

async function forceRefresh() {
  EventUtils.synthesizeKey("R", { accelKey: true, shiftKey: true });
}

async function done() {
  // unregister window actors
  ChromeUtils.unregisterWindowActor("ForceRefresh");
  let tab = gBrowser.selectedTab;
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await ContentTask.spawn(tabBrowser, null, async function() {
    const swr = await content.navigator.serviceWorker.getRegistration();
    await swr.unregister();
  });

  BrowserTestUtils.removeTab(tab);
  executeSoon(finish);
}

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv(
    {
      set: [
        ["dom.serviceWorkers.enabled", true],
        ["dom.serviceWorkers.exemptFromPerDomainMax", true],
        ["dom.serviceWorkers.testing.enabled", true],
        ["dom.caches.enabled", true],
        ["browser.cache.disk.enable", false],
        ["browser.cache.memory.enable", false],
      ],
    },
    async function() {
      // create ForceRefreseh window actor
      const { ForceRefreshParent } = ChromeUtils.import(
        getRootDirectory(gTestPath) + "ForceRefreshParent.jsm"
      );

      // setup helper functions for ForceRefreshParent
      ForceRefreshParent.SimpleTest = SimpleTest;
      ForceRefreshParent.refresh = refresh;
      ForceRefreshParent.forceRefresh = forceRefresh;
      ForceRefreshParent.done = done;

      // setup window actor options
      let windowActorOptions = {
        parent: {
          moduleURI: getRootDirectory(gTestPath) + "ForceRefreshParent.jsm",
        },
        child: {
          moduleURI: getRootDirectory(gTestPath) + "ForceRefreshChild.jsm",
          events: {
            "base-register": { capture: true, wantUntrusted: true },
            "base-sw-ready": { capture: true, wantUntrusted: true },
            "base-load": { capture: true, wantUntrusted: true },
            "cached-load": { capture: true, wantUntrusted: true },
            "cached-failure": { capture: true, wantUntrusted: true },
          },
        },
        allFrames: true,
      };

      // register ForceRefresh window actors
      ChromeUtils.registerWindowActor("ForceRefresh", windowActorOptions);

      // create a new tab and load test url
      var url = gTestRoot + "browser_base_force_refresh.html";
      var tab = BrowserTestUtils.addTab(gBrowser);
      var tabBrowser = gBrowser.getBrowserForTab(tab);
      gBrowser.selectedTab = tab;
      BrowserTestUtils.loadURI(gBrowser, url);
    }
  );
}
