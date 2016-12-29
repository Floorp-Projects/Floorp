/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../contentSearchUI.js */

// The process of adding a new default snippet involves:
//   * add a new entity to aboutHome.dtd
//   * add a <span/> for it in aboutHome.xhtml
//   * add an entry here in the proper ordering (based on spans)
// The <a/> part of the snippet will be linked to the corresponding url.
const DEFAULT_SNIPPETS_URLS = [
  "https://www.mozilla.org/firefox/features/?utm_source=snippet&utm_medium=snippet&utm_campaign=default+feature+snippet"
, "https://addons.mozilla.org/firefox/?utm_source=snippet&utm_medium=snippet&utm_campaign=addons"
];

const SNIPPETS_UPDATE_INTERVAL_MS = 14400000; // 4 hours.

// IndexedDB storage constants.
const DATABASE_NAME = "abouthome";
const DATABASE_VERSION = 1;
const DATABASE_STORAGE = "persistent";
const SNIPPETS_OBJECTSTORE_NAME = "snippets";
var searchText;

// This global tracks if the page has been set up before, to prevent double inits
var gInitialized = false;
var gObserver = new MutationObserver(function(mutations) {
  for (let mutation of mutations) {
    // The addition of the restore session button changes our width:
    if (mutation.attributeName == "session") {
      fitToWidth();
    }
    if (mutation.attributeName == "snippetsVersion") {
      if (!gInitialized) {
        ensureSnippetsMapThen(loadSnippets);
        gInitialized = true;
      }
      return;
    }
  }
});

window.addEventListener("pageshow", function() {
  // Delay search engine setup, cause browser.js::BrowserOnAboutPageLoad runs
  // later and may use asynchronous getters.
  window.gObserver.observe(document.documentElement, { attributes: true });
  window.gObserver.observe(document.getElementById("launcher"), { attributes: true });
  fitToWidth();
  setupSearch();
  window.addEventListener("resize", fitToWidth);

  // Ask chrome to update snippets.
  var event = new CustomEvent("AboutHomeLoad", {bubbles:true});
  document.dispatchEvent(event);
});

window.addEventListener("pagehide", function() {
  window.gObserver.disconnect();
  window.removeEventListener("resize", fitToWidth);
});

window.addEventListener("keypress", ev => {
  if (ev.defaultPrevented) {
    return;
  }

  // don't focus the search-box on keypress if something other than the
  // body or document element has focus - don't want to steal input from other elements
  // Make an exception for <a> and <button> elements (and input[type=button|submit])
  // which don't usefully take keypresses anyway.
  // (except space, which is handled below)
  if (document.activeElement && document.activeElement != document.body &&
      document.activeElement != document.documentElement &&
      !["a", "button"].includes(document.activeElement.localName) &&
      !document.activeElement.matches("input:-moz-any([type=button],[type=submit])")) {
    return;
  }

  let modifiers = ev.ctrlKey + ev.altKey + ev.metaKey;
  // ignore Ctrl/Cmd/Alt, but not Shift
  // also ignore Tab, Insert, PageUp, etc., and Space
  if (modifiers != 0 || ev.charCode == 0 || ev.charCode == 32)
    return;

  searchText.focus();
  // need to send the first keypress outside the search-box manually to it
  searchText.value += ev.key;
});

// This object has the same interface as Map and is used to store and retrieve
// the snippets data.  It is lazily initialized by ensureSnippetsMapThen(), so
// be sure its callback returned before trying to use it.
var gSnippetsMap;
var gSnippetsMapCallbacks = [];

/**
 * Ensure the snippets map is properly initialized.
 *
 * @param aCallback
 *        Invoked once the map has been initialized, gets the map as argument.
 * @note Snippets should never directly manage the underlying storage, since
 *       it may change inadvertently.
 */
function ensureSnippetsMapThen(aCallback)
{
  if (gSnippetsMap) {
    aCallback(gSnippetsMap);
    return;
  }

  // Handle multiple requests during the async initialization.
  gSnippetsMapCallbacks.push(aCallback);
  if (gSnippetsMapCallbacks.length > 1) {
    // We are already updating, the callbacks will be invoked when done.
    return;
  }

  let invokeCallbacks = function() {
    if (!gSnippetsMap) {
      gSnippetsMap = Object.freeze(new Map());
    }

    for (let callback of gSnippetsMapCallbacks) {
      callback(gSnippetsMap);
    }
    gSnippetsMapCallbacks.length = 0;
  }

  let openRequest = indexedDB.open(DATABASE_NAME, {version: DATABASE_VERSION,
                                                   storage: DATABASE_STORAGE});

  openRequest.onerror = function(event) {
    // Try to delete the old database so that we can start this process over
    // next time.
    indexedDB.deleteDatabase(DATABASE_NAME);
    invokeCallbacks();
  };

  openRequest.onupgradeneeded = function(event) {
    let db = event.target.result;
    if (!db.objectStoreNames.contains(SNIPPETS_OBJECTSTORE_NAME)) {
      db.createObjectStore(SNIPPETS_OBJECTSTORE_NAME);
    }
  }

  openRequest.onsuccess = function(event) {
    let db = event.target.result;

    db.onerror = function() {
      invokeCallbacks();
    }

    db.onversionchange = function(versionChangeEvent) {
      versionChangeEvent.target.close();
      invokeCallbacks();
    }

    let cache = new Map();
    let cursorRequest;
    try {
      cursorRequest = db.transaction(SNIPPETS_OBJECTSTORE_NAME)
                        .objectStore(SNIPPETS_OBJECTSTORE_NAME).openCursor();
    } catch (ex) {
      console.error(ex);
      invokeCallbacks();
      return;
    }

    cursorRequest.onerror = function() {
      invokeCallbacks();
    }

    cursorRequest.onsuccess = function(cursorRequestEvent) {
      let cursor = cursorRequestEvent.target.result;

      // Populate the cache from the persistent storage.
      if (cursor) {
        cache.set(cursor.key, cursor.value);
        cursor.continue();
        return;
      }

      // The cache has been filled up, create the snippets map.
      gSnippetsMap = Object.freeze({
        get: (aKey) => cache.get(aKey),
        set: function(aKey, aValue) {
          db.transaction(SNIPPETS_OBJECTSTORE_NAME, "readwrite")
            .objectStore(SNIPPETS_OBJECTSTORE_NAME).put(aValue, aKey);
          return cache.set(aKey, aValue);
        },
        has: (aKey) => cache.has(aKey),
        delete: function(aKey) {
          db.transaction(SNIPPETS_OBJECTSTORE_NAME, "readwrite")
            .objectStore(SNIPPETS_OBJECTSTORE_NAME).delete(aKey);
          return cache.delete(aKey);
        },
        clear: function() {
          db.transaction(SNIPPETS_OBJECTSTORE_NAME, "readwrite")
            .objectStore(SNIPPETS_OBJECTSTORE_NAME).clear();
          return cache.clear();
        },
        get size() { return cache.size; },
      });

      setTimeout(invokeCallbacks, 0);
    }
  }
}

function onSearchSubmit(aEvent)
{
  gContentSearchController.search(aEvent);
}


var gContentSearchController;

function setupSearch()
{
  // Set submit button label for when CSS background are disabled (e.g.
  // high contrast mode).
  document.getElementById("searchSubmit").value =
    document.body.getAttribute("dir") == "ltr" ? "\u25B6" : "\u25C0";

  // The "autofocus" attribute doesn't focus the form element
  // immediately when the element is first drawn, so the
  // attribute is also used for styling when the page first loads.
  searchText = document.getElementById("searchText");
  searchText.addEventListener("blur", function searchText_onBlur() {
    searchText.removeEventListener("blur", searchText_onBlur);
    searchText.removeAttribute("autofocus");
  });

  if (!gContentSearchController) {
    gContentSearchController =
      new ContentSearchUIController(searchText, searchText.parentNode,
                                    "abouthome", "homepage");
  }
}

/**
 * Inform the test harness that we're done loading the page.
 */
function loadCompleted()
{
  var event = new CustomEvent("AboutHomeLoadSnippetsCompleted", {bubbles:true});
  document.dispatchEvent(event);
}

/**
 * Update the local snippets from the remote storage, then show them through
 * showSnippets.
 */
function loadSnippets()
{
  if (!gSnippetsMap)
    throw new Error("Snippets map has not properly been initialized");

  // Allow tests to modify the snippets map before using it.
  var event = new CustomEvent("AboutHomeLoadSnippets", {bubbles:true});
  document.dispatchEvent(event);

  // Check cached snippets version.
  let cachedVersion = gSnippetsMap.get("snippets-cached-version") || 0;
  let currentVersion = document.documentElement.getAttribute("snippetsVersion");
  if (cachedVersion < currentVersion) {
    // The cached snippets are old and unsupported, restart from scratch.
    gSnippetsMap.clear();
  }

  // Check last snippets update.
  let lastUpdate = gSnippetsMap.get("snippets-last-update");
  let updateURL = document.documentElement.getAttribute("snippetsURL");
  let shouldUpdate = !lastUpdate ||
                     Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS;
  if (updateURL && shouldUpdate) {
    // Try to update from network.
    let xhr = new XMLHttpRequest();
    xhr.timeout = 5000;
    // Even if fetching should fail we don't want to spam the server, thus
    // set the last update time regardless its results.  Will retry tomorrow.
    gSnippetsMap.set("snippets-last-update", Date.now());
    xhr.onloadend = function() {
      if (xhr.status == 200) {
        gSnippetsMap.set("snippets", xhr.responseText);
        gSnippetsMap.set("snippets-cached-version", currentVersion);
      }
      showSnippets();
      loadCompleted();
    };
    try {
      xhr.open("GET", updateURL, true);
      xhr.send(null);
    } catch (ex) {
      showSnippets();
      loadCompleted();
      return;
    }
  } else {
    showSnippets();
    loadCompleted();
  }
}

/**
 * Shows locally cached remote snippets, or default ones when not available.
 *
 * @note: snippets should never invoke showSnippets(), or they may cause
 *        a "too much recursion" exception.
 */
var _snippetsShown = false;
function showSnippets()
{
  let snippetsElt = document.getElementById("snippets");

  // Show about:rights notification, if needed.
  let showRights = document.documentElement.getAttribute("showKnowYourRights");
  if (showRights) {
    let rightsElt = document.getElementById("rightsSnippet");
    let anchor = rightsElt.getElementsByTagName("a")[0];
    anchor.href = "about:rights";
    snippetsElt.appendChild(rightsElt);
    rightsElt.removeAttribute("hidden");
    return;
  }

  if (!gSnippetsMap)
    throw new Error("Snippets map has not properly been initialized");
  if (_snippetsShown) {
    // There's something wrong with the remote snippets, just in case fall back
    // to the default snippets.
    showDefaultSnippets();
    throw new Error("showSnippets should never be invoked multiple times");
  }
  _snippetsShown = true;

  let snippets = gSnippetsMap.get("snippets");
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

  showDefaultSnippets();
}

/**
 * Clear snippets element contents and show default snippets.
 */
function showDefaultSnippets()
{
  // Clear eventual contents...
  let snippetsElt = document.getElementById("snippets");
  snippetsElt.innerHTML = "";

  // ...then show default snippets.
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
  if (document.documentElement.scrollWidth > window.innerWidth) {
    document.body.setAttribute("narrow", "true");
  } else if (document.body.hasAttribute("narrow")) {
    document.body.removeAttribute("narrow");
    fitToWidth();
  }
}
