/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// If a definition requires additional params, check that the final search url
// is handled correctly by the engine.
const SEARCH_ENGINES = {
  "Google": {
    image: "data:image/png;base64," +
           "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"
#ifdef XP_MACOSX
  , imageHD: "data:image/png;base64,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"
#endif
  , params: "source=hp&channel=np"
  }
};

// The process of adding a new default snippet involves:
//   * add a new entity to aboutHome.dtd
//   * add a <span/> for it in aboutHome.xhtml
//   * add an entry here in the proper ordering (based on spans)
// The <a/> part of the snippet will be linked to the corresponding url.
const DEFAULT_SNIPPETS_URLS = [
  "http://www.mozilla.com/firefox/features/?WT.mc_ID=default1"
, "https://addons.mozilla.org/firefox/?src=snippet&WT.mc_ID=default2"
];

const SNIPPETS_UPDATE_INTERVAL_MS = 86400000; // 1 Day.

let gObserver = new MutationObserver(function (mutations) {
  for (let mutation of mutations) {
    if (mutation.attributeName == "searchEngineURL") {
      gObserver.disconnect();
      setupSearchEngine();
      loadSnippets();
      return;
    }
  }
});

window.addEventListener("load", function () {
  // Delay search engine setup, cause browser.js::BrowserOnAboutPageLoad runs
  // later and may use asynchronous getters.
  window.gObserver.observe(document.documentElement, { attributes: true });
  fitToWidth();
  window.addEventListener("resize", fitToWidth);
});

function onSearchSubmit(aEvent)
{
  let searchTerms = document.getElementById("searchText").value;
  let searchURL = document.documentElement.getAttribute("searchEngineURL");
  if (searchURL && searchTerms.length > 0) {
    const SEARCH_TOKENS = {
      "_searchTerms_": encodeURIComponent(searchTerms)
    }
    for (let key in SEARCH_TOKENS) {
      searchURL = searchURL.replace(key, SEARCH_TOKENS[key]);
    }
    window.location.href = searchURL;
  }

  aEvent.preventDefault();
}


function setupSearchEngine()
{
  let searchEngineName = document.documentElement.getAttribute("searchEngineName");
  let searchEngineInfo = SEARCH_ENGINES[searchEngineName];
  if (!searchEngineInfo) {
    return;
  }

  // Enqueue additional params if required by the engine definition.
  if (searchEngineInfo.params) {
    let searchEngineURL = document.documentElement.getAttribute("searchEngineURL");
    searchEngineURL += "&" + searchEngineInfo.params;
    document.documentElement.setAttribute("searchEngineURL", searchEngineURL);
  }

  // Add search engine logo.
  if (searchEngineInfo.image) {
    let logoElt = document.getElementById("searchEngineLogo");
    logoElt.src = searchEngineInfo.image;
#ifdef XP_MACOSX
    if (searchEngineInfo.imageHD && window.matchMedia("(min-resolution: 2dppx)"))
      logoElt.src = searchEngineInfo.imageHD;
#endif
    logoElt.alt = searchEngineInfo.name;
  }

  // The "autofocus" attribute doesn't focus the form element
  // immediately when the element is first drawn, so the
  // attribute is also used for styling when the page first loads.
  let searchText = document.getElementById("searchText");
  searchText.addEventListener("blur", function searchText_onBlur() {
    searchText.removeEventListener("blur", searchText_onBlur);
    searchText.removeAttribute("autofocus");
  });

}

function loadSnippets()
{
  // Check last snippets update.
  let lastUpdate = localStorage["snippets-last-update"];
  let updateURL = document.documentElement.getAttribute("snippetsURL");
  if (updateURL && (!lastUpdate ||
                    Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS)) {
    // Try to update from network.
    let xhr = new XMLHttpRequest();
    try {
      xhr.open("GET", updateURL, true);
    } catch (ex) {
      showSnippets();
      return;
    }
    // Even if fetching should fail we don't want to spam the server, thus
    // set the last update time regardless its results.  Will retry tomorrow.
    localStorage["snippets-last-update"] = Date.now();
    xhr.onerror = function (event) {
      showSnippets();
    };
    xhr.onload = function (event)
    {
      if (xhr.status == 200) {
        localStorage["snippets"] = xhr.responseText;
      }
      showSnippets();
    };
    xhr.send(null);
  } else {
    showSnippets();
  }
}

function showSnippets()
{
  let snippetsElt = document.getElementById("snippets");
  let snippets = localStorage["snippets"];
  // If there are remotely fetched snippets, try to to show them.
  if (snippets) {
    // Injecting snippets can throw if they're invalid XML.
    try {
      snippetsElt.innerHTML = snippets;
      // Scripts injected by innerHTML are inactive, so we have to relocate them
      // through DOM manipulation to activate their contents.
      Array.forEach(snippetsElt.getElementsByTagName("script"), function(elt) {
        let relocatedScript = document.createElement("script");
        relocatedScript.type = "text/javascript;version=1.8";
        relocatedScript.text = elt.text;
        elt.parentNode.replaceChild(relocatedScript, elt);
      });
      return;
    } catch (ex) {
      // Bad content, continue to show default snippets.
    }
  }

  // Show default snippets otherwise.
  let defaultSnippetsElt = document.getElementById("defaultSnippets");
  let entries = defaultSnippetsElt.querySelectorAll("span");
  // Choose a random snippet.  Assume there is always at least one.
  let randIndex = Math.floor(Math.random() * entries.length);
  let entry = entries[randIndex];
  // Inject url in the eventual link.
  if (DEFAULT_SNIPPETS_URLS[randIndex]) {
    let links = entry.getElementsByTagName("a");
    // Default snippets can have only one link, otherwise something is messed
    // up in the translation.
    if (links.length == 1) {
      links[0].href = DEFAULT_SNIPPETS_URLS[randIndex];
    }
  }
  // Move the default snippet to the snippets element.
  snippetsElt.appendChild(entry);
}

function fitToWidth() {
  if (window.scrollMaxX) {
    document.body.setAttribute("narrow", "true");
  } else if (document.body.hasAttribute("narrow")) {
    document.body.removeAttribute("narrow");
    fitToWidth();
  }
}
