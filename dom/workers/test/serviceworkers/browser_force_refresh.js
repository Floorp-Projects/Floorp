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
    sendAsyncMessage("test:event", {type: event.type});
  }

  // These are tab-local, so no need to unregister them.
  addEventListener('base-load', eventHandler, true, true);
  addEventListener('base-register', eventHandler, true, true);
  addEventListener('base-sw-ready', eventHandler, true, true);
  addEventListener('cached-load', eventHandler, true, true);
}

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({'set': [['dom.serviceWorkers.enabled', true],
                                     ['dom.serviceWorkers.exemptFromPerDomainMax', true],
                                     ['dom.serviceWorkers.testing.enabled', true],
                                     ['dom.serviceWorkers.interception.enabled', true],
                                     ['dom.caches.enabled', true]]},
                            function() {
    var url = gTestRoot + 'browser_base_force_refresh.html';
    var tab = gBrowser.addTab();
    gBrowser.selectedTab = tab;

    tab.linkedBrowser.messageManager.loadFrameScript("data:,(" + encodeURIComponent(frameScript) + ")()", true);
    gBrowser.loadURI(url);

    var cachedLoad = false;

    function eventHandler(msg) {
      if (msg.data.type === 'base-load') {
        if (cachedLoad) {
          tab.linkedBrowser.messageManager.removeMessageListener("test:event", eventHandler);

          gBrowser.removeTab(tab);
          executeSoon(finish);
        }
      } else if (msg.data.type === 'base-register') {
        ok(!cachedLoad, 'cached load should not occur before base register');
        refresh();
      } else if (msg.data.type === 'base-sw-ready') {
        ok(!cachedLoad, 'cached load should not occur before base ready');
        refresh();
      } else if (msg.data.type === 'cached-load') {
        ok(!cachedLoad, 'cached load should not occur twice');
        cachedLoad = true;
        forceRefresh();
      }

      return;
    }

    tab.linkedBrowser.messageManager.addMessageListener("test:event", eventHandler);
  });
}
