/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

registerCleanupFunction(function() {
  // Ensure we don't pollute prefs for next tests.
  try {
    Services.prefs.clearUserPref("network.cookies.cookieBehavior");
  } catch (ex) {}
  try {
    Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
  } catch (ex) {}
  try {
    Services.prefs.clearUserPref("services.sync.username");
  } catch (ex) {}
});

let gTests = [

{
  desc: "Check that rejecting cookies does not prevent page from working",
  setup: function ()
  {
    Services.prefs.setIntPref("network.cookies.cookieBehavior", 2);
  },
  run: function ()
  {
    let storage = getStorage();
    isnot(storage.getItem("search-engine"), null);
    try {
      Services.prefs.clearUserPref("network.cookies.cookieBehavior");
    } catch (ex) {}
    executeSoon(runNextTest);
  }
},

{
  desc: "Check that asking for cookies does not prevent page from working",
  setup: function ()
  {
    Services.prefs.setIntPref("network.cookie.lifetimePolicy", 1);
  },
  run: function ()
  {
    let storage = getStorage();
    isnot(storage.getItem("search-engine"), null);
    try {
      Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
    } catch (ex) {}
    executeSoon(runNextTest);
  }
},

{
  desc: "Check that clearing cookies does not prevent page from working",
  setup: function ()
  {
    Components.classes["@mozilla.org/dom/storagemanager;1"].
    getService(Components.interfaces.nsIObserver).
    observe(null, "cookie-changed", "cleared");
  },
  run: function ()
  {
    let storage = getStorage();
    isnot(storage.getItem("search-engine"), null);
    executeSoon(runNextTest);
  }
},

{
  desc: "Check normal status is working",
  setup: function ()
  {
  },
  run: function ()
  {
    let storage = getStorage();
    isnot(storage.getItem("search-engine"), null);
    executeSoon(runNextTest);
  }
},

{
  desc: "Check default snippets are shown",
  setup: function ()
  {
  },
  run: function ()
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element")
    is(snippetsElt.getElementsByTagName("span").length, 1,
       "A default snippet is visible.");
    executeSoon(runNextTest);
  }
},

{
  desc: "Check default snippets are shown if snippets are invalid xml",
  setup: function ()
  {
    let storage = getStorage();
    // This must be some incorrect xhtml code.
    storage.setItem("snippets", "<p><b></p></b>");
  },
  run: function ()
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;

    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    is(snippetsElt.getElementsByTagName("span").length, 1,
       "A default snippet is visible.");

    executeSoon(runNextTest);
  }
},

{
  desc: "Check sync links visibility before and after Sync setup",
  setup: function ()
  {
    try {
      Services.prefs.clearUserPref("services.sync.username");
    } catch (ex) {}
    Services.obs.notifyObservers(null, "weave:service:ready", null);
  },
  run: function ()
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let pairLink = doc.getElementById("pairDeviceLink");
    let setupLink = doc.getElementById("setupSyncLink");

    ok(pairLink, "Found 'Pair Device' link");
    ok(setupLink, "Found 'Set Up Sync' link");
    ok(!pairLink.hidden, "'Pair' link is visible before setup");
    ok(!setupLink.hidden, "'Set Up' link is visible before setup");

    Services.obs.notifyObservers(null, "weave:service:setup-complete", null);

    executeSoon(function () {
      setupLink = doc.getElementById("setupSyncLink");
      ok(setupLink.hidden, "'Set Up' link is hidden after setup");
      ok(!pairLink.hidden, "'Pair' link is visible after setup");

      executeSoon(runNextTest);
    });
  }
},

{
  desc: "Check sync links visibility before and after Sync unlink",
  setup: function ()
  {
    Services.prefs.setCharPref("services.sync.username", "someuser@domain.com");
    Services.obs.notifyObservers(null, "weave:service:ready", null);
  },
  run: function ()
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let pairLink = doc.getElementById("pairDeviceLink");
    let setupLink = doc.getElementById("setupSyncLink");

    ok(!pairLink.hidden, "'Pair' link is visible before unlink");
    ok(setupLink.hidden, "'Set Up' link is hidden before unlink");

    Services.obs.notifyObservers(null, "weave:service:start-over", null);

    executeSoon(function () {
      setupLink = doc.getElementById("setupSyncLink");
      ok(!setupLink.hidden, "'Set Up' link is visible after unlink");
      ok(!pairLink.hidden, "'Pair' link is visible after unlink");
      executeSoon(runNextTest);
    });
  }
},

{
  desc: "Check Pair Device link opens correct dialog with Sync account ",
  setup: function ()
  {
    Services.prefs.setCharPref("services.sync.username", "someuser@domain.com");
    Services.obs.notifyObservers(null, "weave:service:ready", null);
  },
  run: function ()
  {
    expectDialogWindow("Sync:AddDevice");
    let browser = gBrowser.selectedTab.linkedBrowser;
    let button = browser.contentDocument.getElementById("pairDeviceLink");
    EventUtils.sendMouseEvent({type: "click"}, button, browser.contentWindow);
  }
},

{
  desc: "Check Pair Device link opens correct dialog without Sync account",
  setup: function ()
  {
    try {
      Services.prefs.clearUserPref("services.sync.username");
    } catch (ex) {}
    Services.obs.notifyObservers(null, "weave:service:ready", null);
  },
  run: function ()
  {
    expectDialogWindow("Weave:AccountSetup");
    let browser = gBrowser.selectedTab.linkedBrowser;
    let button = browser.contentDocument.getElementById("pairDeviceLink");
    EventUtils.sendMouseEvent({type: "click"}, button, browser.contentWindow);
  }
},

{
  desc: "Check Sync Setup link opens correct dialog (without Sync account)",
  setup: function ()
  {
    try {
      Services.prefs.clearUserPref("services.sync.username");
    } catch (ex) {}
    Services.obs.notifyObservers(null, "weave:service:ready", null);
  },
  run: function ()
  {
    expectDialogWindow("Weave:AccountSetup");
    let browser = gBrowser.selectedTab.linkedBrowser;
    let button = browser.contentDocument.getElementById("setupSyncLink");
    EventUtils.sendMouseEvent({type: "click"}, button, browser.contentWindow);
  }
},
];

function test()
{
  waitForExplicitFinish();

  // browser-chrome test harness inits browser specifying an hardcoded page
  // and this causes nsIBrowserHandler.defaultArgs to not be evaluated since
  // there is a predefined argument.
  // About:home localStorage is populated with overridden homepage, that is
  // setup in the defaultArgs getter.
  // Thus to populate about:home we need to get defaultArgs manually.
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  // Ensure that by default we don't try to check for remote snippets since that
  // could be tricky due to network bustages or slowness.
  let storage = getStorage();
  storage.setItem("snippets-last-update", Date.now());
  storage.removeItem("snippets");

  executeSoon(runNextTest);
}

function runNextTest()
{
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }

  if (gTests.length) {
    let test = gTests.shift();
    info(test.desc);
    test.setup();
    let tab = gBrowser.selectedTab = gBrowser.addTab("about:home");
    tab.linkedBrowser.addEventListener("load", function (event) {
      tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      // Some part of the page is populated on load, so enqueue on it.
      executeSoon(test.run);
    }, true);
  }
  else {
    finish();
  }
}

function expectDialogWindow(expectedDialog) {
  Services.ww.registerNotification(function onWindow(subject, topic) {
    let win = subject.QueryInterface(Components.interfaces.nsIDOMWindow);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      let wintype = win.document.documentElement.getAttribute("windowtype");
      if (topic == "domwindowopened" && wintype == expectedDialog) {
        Services.ww.unregisterNotification(onWindow);
        // Clean up dialog.
        win.close();
        executeSoon(runNextTest);
      }
    }, false);
  });
}

function getStorage()
{
  let aboutHomeURI = Services.io.newURI("moz-safe-about:home", null, null);
  let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"].
                  getService(Components.interfaces.nsIScriptSecurityManager).
                  getCodebasePrincipal(Services.io.newURI("about:home", null, null));
  let dsm = Components.classes["@mozilla.org/dom/storagemanager;1"].
            getService(Components.interfaces.nsIDOMStorageManager);
  return dsm.getLocalStorageForPrincipal(principal, "");
};
