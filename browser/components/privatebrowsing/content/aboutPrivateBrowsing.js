/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

document.addEventListener("DOMContentLoaded", function() {
  if (!RPMIsWindowPrivate()) {
    document.documentElement.classList.remove("private");
    document.documentElement.classList.add("normal");
    document
      .getElementById("startPrivateBrowsing")
      .addEventListener("click", function() {
        RPMSendAsyncMessage("OpenPrivateWindow");
      });
    return;
  }

  // Setup the private browsing myths link.
  document
    .getElementById("private-browsing-myths")
    .setAttribute(
      "href",
      RPMGetFormatURLPref("app.support.baseURL") + "private-browsing-myths"
    );

  // Set up the private search banner.
  const privateSearchBanner = document.getElementById("search-banner");

  RPMSendQuery("ShouldShowSearchBanner", {}).then(engineName => {
    if (engineName) {
      document.l10n.setAttributes(
        document.getElementById("about-private-browsing-search-banner-title"),
        "about-private-browsing-search-banner-title",
        { engineName }
      );
      privateSearchBanner.removeAttribute("hidden");
      document.body.classList.add("showBanner");
    }

    // We set this attribute so that tests know when we are done.
    document.documentElement.setAttribute("SearchBannerInitialized", true);
  });

  function hideSearchBanner() {
    privateSearchBanner.setAttribute("hidden", "true");
    document.body.classList.remove("showBanner");
    RPMSendAsyncMessage("SearchBannerDismissed");
  }

  document
    .getElementById("search-banner-close-button")
    .addEventListener("click", () => {
      hideSearchBanner();
    });

  let openSearchOptions = document.getElementById(
    "about-private-browsing-search-banner-description"
  );
  let openSearchOptionsEvtHandler = evt => {
    if (
      evt.target.id == "open-search-options-link" &&
      (evt.keyCode == evt.DOM_VK_RETURN || evt.type == "click")
    ) {
      RPMSendAsyncMessage("OpenSearchPreferences");
      hideSearchBanner();
    }
  };
  openSearchOptions.addEventListener("click", openSearchOptionsEvtHandler);
  openSearchOptions.addEventListener("keypress", openSearchOptionsEvtHandler);

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
    RPMSendAsyncMessage(SEARCH_HANDOFF_TOPIC, { text });
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
  new window.ContentSearchHandoffUIController();
});
