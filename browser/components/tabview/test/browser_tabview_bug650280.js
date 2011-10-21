/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);

function test() {
  let cw;

  registerCleanupFunction(function() {
    if (cw)
      cw.Search.hide();

    TabView.hide();
    pb.privateBrowsingEnabled = false;
  });

  let enableSearch = function (callback) {
    if (cw.Search.isEnabled()) {
      callback();
      return;
    }

    cw.addEventListener("tabviewsearchenabled", function onSearchEnabled() {
      cw.removeEventListener("tabviewsearchenabled", onSearchEnabled, false);
      executeSoon(callback);
    }, false);

    cw.Search.ensureShown();
  };

  let getSearchboxValue = function () {
    return cw.iQ("#searchbox").val();
  };

  let prepareSearchbox = function (callback) {
    ok(!cw.Search.isEnabled(), "search is disabled");

    enableSearch(function () {
      cw.iQ("#searchbox").val("moz");
      callback();
    });
  };

  let searchAndSwitchPBMode = function (callback) {
    prepareSearchbox(function () {
      togglePrivateBrowsing(function () {
        showTabView(function () {
          ok(!cw.Search.isEnabled(), "search is disabled");
          is(getSearchboxValue(), "", "search box is empty");
          callback();
        });
      });
    });
  };

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    searchAndSwitchPBMode(function () {
      searchAndSwitchPBMode(function () {
        hideTabView(finish);
      });
    });
  });
}
