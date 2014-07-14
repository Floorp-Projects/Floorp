/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;

const kSearchEngineID = "browser_urifixup_search_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}";
Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                     kSearchEngineURL);

let oldDefaultEngine = Services.search.defaultEngine;
Services.search.defaultEngine = Services.search.getEngineByName(kSearchEngineID);

let tab;
let searchParams;

function checkURL() {
  let escapedParams = encodeURIComponent(searchParams).replace("%20", "+");
  let expectedURL = kSearchEngineURL.replace("{searchTerms}", escapedParams);
  is(tab.linkedBrowser.currentURI.spec, expectedURL,
     "New tab should have loaded with expected url.");
}

function addPageShowListener(aFunc) {
  gBrowser.selectedBrowser.addEventListener("pageshow", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", loadListener, false);
    aFunc();
  });
}

function locationBarEnter(aCallback) {
  executeSoon(function() {
    gURLBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", {});
    addPageShowListener(aCallback);
  });
}

let urlbarInput = [
  "foo bar",
  "brokenprotocol:somethingelse"
];
function test() {
  waitForExplicitFinish();

  nextTest();
}

function nextTest() {
  searchParams = urlbarInput.pop();
  tab = gBrowser.selectedTab = gBrowser.addTab();

  gURLBar.value = searchParams;
  locationBarEnter(function() {
    checkURL();
    gBrowser.removeTab(tab);
    tab = null;
    if (urlbarInput.length) {
      nextTest();
    } else {
      finish();
    }
  });
}

registerCleanupFunction(function () {
  if (tab) {
    gBrowser.removeTab(tab);
  }

  if (oldDefaultEngine) {
    Services.search.defaultEngine = oldDefaultEngine;
  }
  let engine = Services.search.getEngineByName(kSearchEngineID);
  if (engine) {
    Services.search.removeEngine(engine);
  }
});

