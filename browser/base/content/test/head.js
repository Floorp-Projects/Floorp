function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

function findChromeWindowByURI(aURI) {
  let windows = Services.wm.getEnumerator(null);
  while (windows.hasMoreElements()) {
    let win = windows.getNext();
    if (win.location.href == aURI)
      return win;
  }
  return null;
}

function updateTabContextMenu(tab) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  menu.hidePopup();
}

function findToolbarCustomizationWindow(aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  let iframe = aBrowserWin.document.getElementById("customizeToolbarSheetIFrame");
  let win = iframe && iframe.contentWindow;
  if (win)
    return win;

  win = findChromeWindowByURI("chrome://global/content/customizeToolbar.xul");
  if (win && win.opener == aBrowserWin)
    return win;

  throw Error("Failed to find the customization window");
}

function openToolbarCustomizationUI(aCallback, aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  aBrowserWin.document.getElementById("cmd_CustomizeToolbars").doCommand();

  aBrowserWin.gNavToolbox.addEventListener("beforecustomization", function UI_loaded() {
    aBrowserWin.gNavToolbox.removeEventListener("beforecustomization", UI_loaded);

    let win = findToolbarCustomizationWindow(aBrowserWin);
    waitForFocus(function () {
      aCallback(win);
    }, win);
  });
}

function closeToolbarCustomizationUI(aCallback, aBrowserWin) {
  let win = findToolbarCustomizationWindow(aBrowserWin);

  win.addEventListener("unload", function unloaded() {
    win.removeEventListener("unload", unloaded);
    executeSoon(aCallback);
  });

  let button = win.document.getElementById("donebutton");
  button.focus();
  button.doCommand();
}

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    if (condition()) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

// Check that a specified (string) URL hasn't been "remembered" (ie, is not
// in history, will not appear in about:newtab or auto-complete, etc.)
function ensureSocialUrlNotRemembered(url) {
  let gh = Cc["@mozilla.org/browser/global-history;2"]
           .getService(Ci.nsIGlobalHistory2);
  let uri = Services.io.newURI(url, null, null);
  ok(!gh.isVisited(uri), "social URL " + url + " should not be in global history");
}

function runSocialTestWithProvider(manifest, callback) {
  let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

  // Check that none of the provider's content ends up in history.
  registerCleanupFunction(function () {
    for (let what of ['sidebarURL', 'workerURL', 'iconURL']) {
      if (manifest[what]) {
        ensureSocialUrlNotRemembered(manifest[what]);
      }
    }
  });

  info("runSocialTestWithProvider: " + manifest.toSource());

  let oldProvider;
  SocialService.addProvider(manifest, function(provider) {
    info("runSocialTestWithProvider: provider added");
    oldProvider = Social.provider;
    Social.provider = provider;

    // Now that we've set the UI's provider, enable the social functionality
    Services.prefs.setBoolPref("social.enabled", true);

    registerCleanupFunction(function () {
      Social.provider = oldProvider;
      Services.prefs.clearUserPref("social.enabled");
    });

    function finishSocialTest() {
      SocialService.removeProvider(provider.origin, finish);
    }
    callback(finishSocialTest);
  });
}


function runSocialTests(tests, cbPreTest, cbPostTest, cbFinish) {
  let testIter = Iterator(tests);

  if (cbPreTest === undefined) {
    cbPreTest = function(cb) {cb()};
  }
  if (cbPostTest === undefined) {
    cbPostTest = function(cb) {cb()};
  }

  function runNextTest() {
    let name, func;
    try {
      [name, func] = testIter.next();
    } catch (err if err instanceof StopIteration) {
      // out of items:
      (cbFinish || finish)();
      return;
    }
    // We run on a timeout as the frameworker also makes use of timeouts, so
    // this helps keep the debug messages sane.
    executeSoon(function() {
      function cleanupAndRunNextTest() {
        info("sub-test " + name + " complete");
        cbPostTest(runNextTest);
      }
      cbPreTest(function() {
        info("sub-test " + name + " starting");
        try {
          func.call(tests, cleanupAndRunNextTest);
        } catch (ex) {
          ok(false, "sub-test " + name + " failed: " + ex.toString() +"\n"+ex.stack);
          cleanupAndRunNextTest();
        }
      })
    });
  }
  runNextTest();
}
