function test() {
  waitForExplicitFinish();

  loadAndWait("data:text/plain,1", function() {
    loadAndWait("data:text/plain,2", function() {
      loadAndWait("data:text/plain,3", runTests);
    });
  });
}

function runTests() {
  duplicate(0, "maintained the original index", function() {
    gBrowser.removeCurrentTab();

    duplicate(-1, "went back", function() {
      duplicate(1, "went forward", function() {
        gBrowser.removeCurrentTab();
        gBrowser.removeCurrentTab();
        gBrowser.addTab();
        gBrowser.removeCurrentTab();
        finish();
      });
    });
  });
}

function duplicate(delta, msg, cb) {
  var start = gBrowser.sessionHistory.index;

  duplicateTabIn(gBrowser.selectedTab, "tab", delta);
  let tab = gBrowser.selectedTab;

  tab.addEventListener("SSTabRestored", function() {
    is(gBrowser.sessionHistory.index, start + delta, msg);
    executeSoon(cb);
  }, {once: true});
}

function loadAndWait(url, cb) {
  gBrowser.selectedBrowser.addEventListener("load", function() {
    executeSoon(cb);
  }, {capture: true, once: true});

  gBrowser.loadURI(url);
}
