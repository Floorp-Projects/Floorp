/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var stateBackup = ss.getBrowserState();

function cleanup() {
  // Reset the pref
  try {
    Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
  } catch (e) {}
  ss.setBrowserState(stateBackup);
  executeSoon(finish);
}

function test() {
  /** Bug 599909 - to-be-reloaded tabs don't show up in switch-to-tab **/
  waitForExplicitFinish();

  // Set the pref to true so we know exactly how many tabs should be restoring at
  // any given time. This guarantees that a finishing load won't start another.
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1" }] },
    { entries: [{ url: "http://example.org/#2" }] },
    { entries: [{ url: "http://example.org/#3" }] },
    { entries: [{ url: "http://example.org/#4" }] }
  ], selected: 1 }] };

  let tabsForEnsure = {};
  state.windows[0].tabs.forEach(function(tab) {
    tabsForEnsure[tab.entries[0].url] = 1;
  });

  let tabsRestoring = 0;
  let tabsRestored = 0;

  function handleEvent(aEvent) {
    if (aEvent.type == "SSTabRestoring")
      tabsRestoring++;
    else
      tabsRestored++;

    if (tabsRestoring < state.windows[0].tabs.length ||
        tabsRestored < 1)
      return;

    gBrowser.tabContainer.removeEventListener("SSTabRestoring", handleEvent, true);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", handleEvent, true);
    executeSoon(function() {
      checkAutocompleteResults(tabsForEnsure, cleanup);
    });
  }

  // currentURI is set before SSTabRestoring is fired, so we can sucessfully check
  // after that has fired for all tabs. Since 1 tab will be restored though, we
  // also need to wait for 1 SSTabRestored since currentURI will be set, unset, then set.
  gBrowser.tabContainer.addEventListener("SSTabRestoring", handleEvent, true);
  gBrowser.tabContainer.addEventListener("SSTabRestored", handleEvent, true);
  ss.setBrowserState(JSON.stringify(state));
}

// The following was taken from browser/base/content/test/general/browser_tabMatchesInAwesomebar.js
// so that we could do the same sort of checking.
var gController = Cc["@mozilla.org/autocomplete/controller;1"].
                  getService(Ci.nsIAutoCompleteController);

function checkAutocompleteResults(aExpected, aCallback) {
  gController.input = {
    timeout: 10,
    textValue: "",
    searches: ["unifiedcomplete"],
    searchParam: "enable-actions",
    popupOpen: false,
    minResultsForPopup: 0,
    invalidate: function() {},
    disableAutoComplete: false,
    completeDefaultIndex: false,
    get popup() { return this; },
    onSearchBegin: function() {},
    onSearchComplete:  function ()
    {
      info("Found " + gController.matchCount + " matches.");
      // Check to see the expected uris and titles match up (in any order)
      for (let i = 0; i < gController.matchCount; i++) {
        if (gController.getStyleAt(i).includes("heuristic")) {
          info("Skip heuristic match");
          continue;
        }
        let action = gURLBar.popup.input._parseActionUrl(gController.getValueAt(i));
        let uri = action.params.url;

        info("Search for '" + uri + "' in open tabs.");
        ok(uri in aExpected, "Registered open page found in autocomplete.");
        // Remove the found entry from expected results.
        delete aExpected[uri];
      }

      // Make sure there is no reported open page that is not open.
      for (let entry in aExpected) {
        ok(false, "'" + entry + "' not found in autocomplete.");
      }

      executeSoon(aCallback);
    },
    setSelectedIndex: function() {},
    get searchCount() { return this.searches.length; },
    getSearchAt: function(aIndex) {
      return this.searches[aIndex];
    },
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIAutoCompleteInput,
      Ci.nsIAutoCompletePopup,
    ])
  };

  info("Searching open pages.");
  gController.startSearch(Services.prefs.getCharPref("browser.urlbar.restrict.openpage"));
}
