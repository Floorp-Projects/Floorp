/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

function test() {
  waitForExplicitFinish();

  runPass("file_bug1108547-2.html", function() {
    runPass("file_bug1108547-3.html", function() {
      finish();
    });
  });
}

function runPass(getterFile, finishedCallback) {
  var rootDir = "http://mochi.test:8888/browser/dom/html/test/";
  var testBrowser;
  var privateWin;

  function whenDelayedStartupFinished(win, callback) {
    let topic = "browser-delayed-startup-finished";
    Services.obs.addObserver(function onStartup(aSubject) {
      if (win != aSubject)
        return;

      Services.obs.removeObserver(onStartup, topic);
      executeSoon(callback);
    }, topic);
  }

  // First, set the cookie in a normal window.
  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug1108547-1.html");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(afterOpenCookieSetter);

  function afterOpenCookieSetter() {
    gBrowser.removeCurrentTab();

    // Now, open a private window.
    privateWin = OpenBrowserWindow({private: true});
      whenDelayedStartupFinished(privateWin, afterPrivateWindowOpened);
  }

  function afterPrivateWindowOpened() {
    // In the private window, open the getter file, and wait for a new tab to be opened.
    privateWin.gBrowser.selectedTab = privateWin.gBrowser.addTab(rootDir + getterFile);
    testBrowser = privateWin.gBrowser.selectedBrowser;
    privateWin.gBrowser.tabContainer.addEventListener("TabOpen", onNewTabOpened, true);
  }

  function fetchResult() {
    return ContentTask.spawn(testBrowser, null, function() {
      return content.document.getElementById("result").textContent;
    });
  }

  function onNewTabOpened() {
    // When the new tab is opened, wait for it to load.
    privateWin.gBrowser.tabContainer.removeEventListener("TabOpen", onNewTabOpened, true);
    BrowserTestUtils.browserLoaded(privateWin.gBrowser.tabs[privateWin.gBrowser.tabs.length - 1].linkedBrowser).then(fetchResult).then(onNewTabLoaded);
  }

  function onNewTabLoaded(result) {
    // Now, ensure that the private tab doesn't have access to the cookie set in normal mode.
    is(result, "", "Shouldn't have access to the cookies");

    // We're done with the private window, close it.
    privateWin.close();

    // Clear all cookies.
    Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager).removeAll();

    // Open a new private window, this time to set a cookie inside it.
    privateWin = OpenBrowserWindow({private: true});
      whenDelayedStartupFinished(privateWin, afterPrivateWindowOpened2);
  }

  function afterPrivateWindowOpened2() {
    // In the private window, open the setter file, and wait for it to load.
    privateWin.gBrowser.selectedTab = privateWin.gBrowser.addTab(rootDir + "file_bug1108547-1.html");
    BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser).then(afterOpenCookieSetter2);
  }

  function afterOpenCookieSetter2() {
    // We're done with the private window now, close it.
    privateWin.close();

    // Now try to read the cookie in a normal window, and wait for a new tab to be opened.
    gBrowser.selectedTab = gBrowser.addTab(rootDir + getterFile);
    testBrowser = gBrowser.selectedBrowser;
    gBrowser.tabContainer.addEventListener("TabOpen", onNewTabOpened2, true);
  }

  function onNewTabOpened2() {
    // When the new tab is opened, wait for it to load.
    gBrowser.tabContainer.removeEventListener("TabOpen", onNewTabOpened2, true);
    BrowserTestUtils.browserLoaded(gBrowser.tabs[gBrowser.tabs.length - 1].linkedBrowser).then(fetchResult).then(onNewTabLoaded2);
  }

  function onNewTabLoaded2(result) {
    // Now, ensure that the normal tab doesn't have access to the cookie set in private mode.
    is(result, "", "Shouldn't have access to the cookies");

    // Remove both of the tabs opened here.
    gBrowser.removeCurrentTab();
    gBrowser.removeCurrentTab();

    privateWin = null;
    testBrowser = null;

    finishedCallback();
  }
}
