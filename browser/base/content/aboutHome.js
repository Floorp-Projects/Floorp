/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is aboutHome.xhtml.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (original author)
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// If a definition requires additional params, check that the final search url
// is handled correctly by the engine.
const SEARCH_ENGINES = {
  "Google": {
    image: "data:image/png;base64," +
           "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"
  , params: "source=hp&channel=np"
  }

, "Яндекс":
  {
    image: "data:image/png;base64," +
           "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"
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

let gSearchEngine;

function onLoad(event)
{
  setupSearchEngine();
  document.getElementById("searchText").focus();

  loadSnippets();
}


function onSearchSubmit(aEvent)
{
  let searchTerms = document.getElementById("searchText").value;
  if (gSearchEngine && searchTerms.length > 0) {
    const SEARCH_TOKENS = {
      "_searchTerms_": encodeURIComponent(searchTerms)
    }
    let url = gSearchEngine.searchUrl;
    for (let key in SEARCH_TOKENS) {
      url = url.replace(key, SEARCH_TOKENS[key]);
    }
    window.location.href = url;
  }

  aEvent.preventDefault();
}


function setupSearchEngine()
{
  gSearchEngine = JSON.parse(localStorage["search-engine"]);

  if (!gSearchEngine)
    return;

  // Look for extended information, like logo and links.
  let searchEngineInfo = SEARCH_ENGINES[gSearchEngine.name];
  if (searchEngineInfo) {
    for (let prop in searchEngineInfo)
      gSearchEngine[prop] = searchEngineInfo[prop];
  }

  // Enqueue additional params if required by the engine definition.
  if (gSearchEngine.params)
    gSearchEngine.searchUrl += "&" + gSearchEngine.params;

  // Add search engine logo.
  if (gSearchEngine.image) {
    let logoElt = document.getElementById("searchEngineLogo");
    logoElt.src = gSearchEngine.image;
    logoElt.alt = gSearchEngine.name;
  }

}

function loadSnippets()
{
  // Check last snippets update.
  let lastUpdate = localStorage["snippets-last-update"];
  let updateURL = localStorage["snippets-update-url"];
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
  let randIndex = Math.round(Math.random() * (entries.length - 1));
  let entry = entries[randIndex];
  // Inject url in the eventual link.
  if (DEFAULT_SNIPPETS_URLS[randIndex]) {
    let links = entry.getElementsByTagName("a");
    // Default snippets can have only one link, otherwise something is messed
    // up in the translation.
    if (links.length == 1) {
      links[0].href = DEFAULT_SNIPPETS_URLS[randIndex];
      activateSnippetsButtonClick(entry);
    }
  }
  // Move the default snippet to the snippets element.
  snippetsElt.appendChild(entry);
}

/**
 * Searches a single link element in aElt and binds its href to the click
 * action of the snippets button.
 *
 * @param aElt
 *        Element to search the link into.
 */
function activateSnippetsButtonClick(aElt) {
  let links = aElt.getElementsByTagName("a");
  if (links.length == 1) {
    document.getElementById("snippets")
            .addEventListener("click", function(aEvent) {
      if (aEvent.target.nodeName != "a")
        window.location = links[0].href;
    }, false);
  }
}
