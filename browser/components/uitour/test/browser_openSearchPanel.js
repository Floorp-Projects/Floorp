/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;

function test() {
  UITourTest();
}

var tests = [
  function test_openSearchPanel(done) {
    // If suggestions are enabled, the panel will attempt to use the network to
    // connect to the suggestions provider, causing the test suite to fail. We
    // also change the preference to display the search bar during the test.
    Services.prefs.setBoolPref("browser.search.widget.inNavBar", true);
    Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
    registerCleanupFunction(() => {
      Services.prefs.clearUserPref("browser.search.widget.inNavBar");
      Services.prefs.clearUserPref("browser.search.suggest.enabled");
    });

    let searchbar = document.getElementById("searchbar");
    ok(!searchbar.textbox.open, "Popup starts as closed");
    gContentAPI.openSearchPanel(() => {
      ok(searchbar.textbox.open, "Popup was opened");
      searchbar.textbox.closePopup();
      ok(!searchbar.textbox.open, "Popup was closed");
      done();
    });
  },
];
