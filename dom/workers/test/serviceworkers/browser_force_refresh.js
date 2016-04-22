/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                    "http://mochi.test:8888/")

function refresh() {
  EventUtils.synthesizeKey('R', { accelKey: true });
}

function forceRefresh() {
  EventUtils.synthesizeKey('R', { accelKey: true, shiftKey: true });
}

function frameScript() {
  function eventHandler(event) {
    sendAsyncMessage("test:event", {type: event.type, detail: event.detail});
  }

  // These are tab-local, so no need to unregister them.
  addEventListener('base-load', eventHandler, true, true);
  addEventListener('base-register', eventHandler, true, true);
  addEventListener('base-sw-ready', eventHandler, true, true);
  addEventListener('cached-load', eventHandler, true, true);
  addEventListener('cached-failure', eventHandler, true, true);
}

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({'set': [['dom.serviceWorkers.enabled', true],
                                     ['dom.serviceWorkers.exemptFromPerDomainMax', true],
                                     ['dom.serviceWorkers.testing.enabled', true],
                                     ['dom.caches.enabled', true],
                                     ['browser.cache.disk.enable', false],
                                     ['browser.cache.memory.enable', false]]},
                            function() {
    var url = gTestRoot + 'browser_base_force_refresh.html';
    var tab = gBrowser.addTab();
    gBrowser.selectedTab = tab;

    tab.linkedBrowser.messageManager.loadFrameScript("data:,(" + encodeURIComponent(frameScript) + ")()", true);
    gBrowser.loadURI(url);

    function done() {
      tab.linkedBrowser.messageManager.removeMessageListener("test:event", eventHandler);

      gBrowser.removeTab(tab);
      executeSoon(finish);
    }

    var maxCacheLoadCount = 3;
    var cachedLoadCount = 0;
    var baseLoadCount = 0;

    function eventHandler(msg) {
      if (msg.data.type === 'base-load') {
        baseLoadCount += 1;
        if (cachedLoadCount === maxCacheLoadCount) {
          is(baseLoadCount, 2, 'cached load should occur before second base load');
          return done();
        }
        if (baseLoadCount !== 1) {
          ok(false, 'base load without cached load should only occur once');
          return done();
        }
      } else if (msg.data.type === 'base-register') {
        ok(!cachedLoadCount, 'cached load should not occur before base register');
        is(baseLoadCount, 1, 'register should occur after first base load');
      } else if (msg.data.type === 'base-sw-ready') {
        ok(!cachedLoadCount, 'cached load should not occur before base ready');
        is(baseLoadCount, 1, 'ready should occur after first base load');
        refresh();
      } else if (msg.data.type === 'cached-load') {
        ok(cachedLoadCount < maxCacheLoadCount, 'cached load should not occur too many times');
        is(baseLoadCount, 1, 'cache load occur after first base load');
        cachedLoadCount += 1;
        if (cachedLoadCount < maxCacheLoadCount) {
          return refresh();
        }
        forceRefresh();
      } else if (msg.data.type === 'cached-failure') {
        ok(false, 'failure: ' + msg.data.detail);
        done();
      }

      return;
    }

    tab.linkedBrowser.messageManager.addMessageListener("test:event", eventHandler);
  });
}
