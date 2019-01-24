/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const TP_PB_ENABLED_PREF = "privacy.trackingprotection.pbmode.enabled";
const PB_SEARCH_UI_ENABLED_PREF = "browser.privatebrowsing.searchUI";

document.addEventListener("DOMContentLoaded", function() {
  if (!RPMIsWindowPrivate()) {
    document.documentElement.classList.remove("private");
    document.documentElement.classList.add("normal");
    document.getElementById("startPrivateBrowsing").addEventListener("click", function() {
      RPMSendAsyncMessage("OpenPrivateWindow");
    });
    return;
  }

  document.getElementById("startTour").addEventListener("click", function() {
    RPMSendAsyncMessage("DontShowIntroPanelAgain");
  });

  let introURL = RPMGetFormatURLPref("privacy.trackingprotection.introURL");
  // Variation 1 is specific to the Content Blocking UI.
  let variation = "?variation=1";

  document.getElementById("startTour").setAttribute("href", introURL + variation);

  document.getElementById("learnMore").setAttribute("href",
    RPMGetFormatURLPref("app.support.baseURL") + "private-browsing");

  let tpEnabled = RPMGetBoolPref(TP_PB_ENABLED_PREF);
  if (!tpEnabled) {
    document.getElementById("tpSubHeader").remove();
    document.getElementById("tpSection").remove();
  }

  let searchUIEnabled = RPMGetBoolPref(PB_SEARCH_UI_ENABLED_PREF);
  if (searchUIEnabled) {
    setupSearchUI();
  }
});

function setupSearchUI() {
  // Show the new search UI and hide the old one.
  document.documentElement.classList.remove("no-search-ui");
  document.documentElement.classList.add("search-ui");

  // Setup the private browsing myths link.
  document.getElementById("private-browsing-myths").setAttribute("href",
    RPMGetFormatURLPref("app.support.baseURL") + "private-browsing-myths");

  // Setup the search hand-off box.
  let btn = document.getElementById("search-handoff-button");
  let editable = document.getElementById("fake-editable");
  let HIDE_SEARCH_TOPIC = "HideSearch";
  let SHOW_SEARCH_TOPIC = "ShowSearch";
  let SEARCH_HANDOFF_TOPIC = "SearchHandoff";

  function showSearch() {
    btn.classList.remove("focused");
    btn.classList.remove("hidden");
    RPMRemoveMessageListener(SHOW_SEARCH_TOPIC, showSearch);
  }

  function hideSearch() {
    btn.classList.add("hidden");
  }

  function handoffSearch(text) {
    RPMSendAsyncMessage(SEARCH_HANDOFF_TOPIC, {text});
    RPMAddMessageListener(SHOW_SEARCH_TOPIC, showSearch);
    if (text) {
      hideSearch();
    } else {
      btn.classList.add("focused");
      RPMAddMessageListener(HIDE_SEARCH_TOPIC, hideSearch);
    }
  }
  btn.addEventListener("focus", function() {
    handoffSearch();
  });
  btn.addEventListener("click", function() {
    handoffSearch();
  });

  // Hand-off any text that gets dropped or pasted
  editable.addEventListener("drop", function(ev) {
    ev.preventDefault();
    let text = ev.dataTransfer.getData("text");
    if (text) {
      handoffSearch(text);
    }
  });
  editable.addEventListener("paste", function(ev) {
    ev.preventDefault();
    handoffSearch(ev.clipboardData.getData("Text"));
  });

  // Load contentSearchUI so it sets the search engine icon for us.
  // TODO: FIXME. We should eventually refector contentSearchUI to do only what
  // we need and have it do the common search handoff work for
  // about:newtab and about:privatebrowsing.
  let input = document.getElementById("dummy-input");
  new window.ContentSearchUIController(input, input.parentNode, "aboutprivatebrowsing", "aboutprivatebrowsing");
}
