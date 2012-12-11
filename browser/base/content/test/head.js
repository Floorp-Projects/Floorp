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

function getTestPlugin() {
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
  ok(false, "Unable to find plugin");
  return null;
}

function runSocialTestWithProvider(manifest, callback) {
  let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

  let manifests = Array.isArray(manifest) ? manifest : [manifest];

  // Check that none of the provider's content ends up in history.
  registerCleanupFunction(function () {
    manifests.forEach(function (m) {
      for (let what of ['sidebarURL', 'workerURL', 'iconURL']) {
        if (m[what]) {
          ensureSocialUrlNotRemembered(m[what]);
        }
      }
    });
  });

  info("runSocialTestWithProvider: " + manifests.toSource());

  let providersAdded = 0;
  let firstProvider;
  manifests.forEach(function (m) {
    SocialService.addProvider(m, function(provider) {
      provider.active = true;

      providersAdded++;
      info("runSocialTestWithProvider: provider added");

      // we want to set the first specified provider as the UI's provider
      if (provider.origin == manifests[0].origin) {
        firstProvider = provider;
      }

      // If we've added all the providers we need, call the callback to start
      // the tests (and give it a callback it can call to finish them)
      if (providersAdded == manifests.length) {
        // Set the UI's provider and enable the feature
        Social.provider = firstProvider;
        Social.enabled = true;

        registerCleanupFunction(function () {
          // disable social before removing the providers to avoid providers
          // being activated immediately before we get around to removing it.
          Services.prefs.clearUserPref("social.enabled");
          // if one test happens to fail, it is likely finishSocialTest will not
          // be called, causing most future social tests to also fail as they
          // attempt to add a provider which already exists - so work
          // around that by also attempting to remove the test provider.
          manifests.forEach(function (m) {
            try {
              SocialService.removeProvider(m.origin, finish);
            } catch (ex) {}
          });
        });
        function finishSocialTest() {
          SocialService.removeProvider(provider.origin, finish);
        }
        callback(finishSocialTest);
      }
    });
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

function updateBlocklist(aCallback) {
  var blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsITimerCallback);
  var observer = function() {
    aCallback();
    Services.obs.removeObserver(observer, "blocklist-updated");
  };
  Services.obs.addObserver(observer, "blocklist-updated", false);
  blocklistNotifier.notify(null);
}

var _originalTestBlocklistURL = null;
function setAndUpdateBlocklist(aURL, aCallback) {
  if (!_originalTestBlocklistURL)
    _originalTestBlocklistURL = Services.prefs.getCharPref("extensions.blocklist.url");
  Services.prefs.setCharPref("extensions.blocklist.url", aURL);
  updateBlocklist(aCallback);
}

function resetBlocklist() {
  Services.prefs.setCharPref("extensions.blocklist.url", _originalTestBlocklistURL);
}

function whenNewWindowLoaded(aOptions, aCallback) {
  let win = OpenBrowserWindow(aOptions);
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    aCallback(win);
  }, false);
}
