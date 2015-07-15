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

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({'set': [['dom.serviceWorkers.enabled', true],
                                     ['dom.serviceWorkers.exemptFromPerDomainMax', true],
                                     ['dom.serviceWorkers.testing.enabled', true],
                                     ['dom.serviceWorkers.interception.enabled', true],
                                     ['dom.caches.enabled', true]]},
                            function() {
    var url = gTestRoot + 'browser_base_force_refresh.html';
    var tab = gBrowser.addTab(url);
    gBrowser.selectedTab = tab;

    var cachedLoad = false;

    function eventHandler(event) {
      if (event.type === 'base-load') {
        if (cachedLoad) {
          gBrowser.removeTab(tab);
          executeSoon(finish);
        }
      } else if (event.type === 'base-register') {
        ok(!cachedLoad, 'cached load should not occur before base register');
        refresh();
      } else if (event.type === 'base-sw-ready') {
        ok(!cachedLoad, 'cached load should not occur before base ready');
        refresh();
      } else if (event.type === 'cached-load') {
        ok(!cachedLoad, 'cached load should not occur twice');
        cachedLoad = true;
        forceRefresh();
      }

      return;
    }

    addEventListener('base-load', eventHandler, true, true);
    addEventListener('base-register', eventHandler, true, true);
    addEventListener('base-sw-ready', eventHandler, true, true);
    addEventListener('cached-load', eventHandler, true, true);
  });
}
