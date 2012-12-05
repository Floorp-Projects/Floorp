/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let enableSearch = function (aCW, aCallback) {
    if (aCW.Search.isEnabled()) {
       aCallback();
      return;
    }

    aCW.addEventListener("tabviewsearchenabled", function onSearchEnabled() {
      aCW.removeEventListener("tabviewsearchenabled", onSearchEnabled, false);
      executeSoon(aCallback);
    }, false);

    aCW.Search.ensureShown();
  };

  let getSearchboxValue = function (aCW) {
    return aCW.iQ("#searchbox").val();
  };

  let prepareSearchbox = function (aCW, aCallback) {
    ok(!aCW.Search.isEnabled(), "search is disabled");

    executeSoon(function() {
      enableSearch(aCW, function() {
        aCW.iQ("#searchbox").val("moz");
        aCallback();
      });
    });
  };

  let searchAndSwitchPBMode = function (aWindow, aCallback) {
    showTabView(function() {
      let cw = aWindow.TabView.getContentWindow();

      prepareSearchbox(cw, function() {
        testOnWindow(!PrivateBrowsingUtils.isWindowPrivate(aWindow), function(win) {
          showTabView(function() {
            let contentWindow = win.TabView.getContentWindow();
            ok(!contentWindow.Search.isEnabled(), "search is disabled");
            is(getSearchboxValue(contentWindow), "", "search box is empty");
            aWindow.TabView.hide();
            win.close();
            hideTabView(function() {
              aWindow.close();
              aCallback();
            }, aWindow);
          }, win);
        });
      });
    }, aWindow);
  };

  let testOnWindow = function(aIsPrivate, aCallback) {
    let win = OpenBrowserWindow({private: aIsPrivate});
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      executeSoon(function() { aCallback(win) });
    }, false);
  }

  waitForExplicitFinish();

  testOnWindow(false, function(win) {
    searchAndSwitchPBMode(win, function() {
      testOnWindow(true, function(win) {
        searchAndSwitchPBMode(win, function() {
          finish();
        });
      });
    });
  });
}
