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
});

let gTests = [

{
  desc: "Check that clearing cookies does not clear storage",
  setup: function ()
  {
    Cc["@mozilla.org/dom/storagemanager;1"]
      .getService(Ci.nsIObserver)
      .observe(null, "cookie-changed", "cleared");
  },
  run: function ()
  {
    let storage = getStorage();
    isnot(storage.getItem("snippets-last-update"), null);
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
    let storage = getStorage();
    storage.removeItem("snippets");
    executeSoon(runNextTest);
  }
},
];

function test()
{
  waitForExplicitFinish();

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
    tab.linkedBrowser.addEventListener("load", function load(event) {
      tab.linkedBrowser.removeEventListener("load", load, true);

      let observer = new MutationObserver(function (mutations) {
        for (let mutation of mutations) {
          if (mutation.attributeName == "searchEngineURL") {
            observer.disconnect();
            executeSoon(test.run);
            return;
          }
        }
      });
      let docElt = tab.linkedBrowser.contentDocument.documentElement;
      observer.observe(docElt, { attributes: true });
    }, true);
  }
  else {
    finish();
  }
}

function getStorage()
{
  let aboutHomeURI = Services.io.newURI("moz-safe-about:home", null, null);
  let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"].
                  getService(Components.interfaces.nsIScriptSecurityManager).
                  getNoAppCodebasePrincipal(Services.io.newURI("about:home", null, null));
  let dsm = Components.classes["@mozilla.org/dom/storagemanager;1"].
            getService(Components.interfaces.nsIDOMStorageManager);
  return dsm.getLocalStorageForPrincipal(principal, "");
};
