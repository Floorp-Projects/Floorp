/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

document.addEventListener("DOMContentLoaded", function(){
  let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
  const searchBox = document.getElementById("searchBox");
  const searchResult = document.getElementById("searchBmsBrowser");
  let searchHistory = [];
  let goHomeButton = document.getElementById("homeButton");
  let searchHistoryListContainer = document.getElementById("searchHistoryListContainer");
  const historyList = document.getElementById("searchHistoryList");

  if (Services.prefs.prefHasUserValue("floorp.search.bms.searchHistory") ) {
    searchHistory = JSON.parse(Services.prefs.getStringPref("floorp.search.bms.searchHistory"));
  }

  async function init() {
    searchBox.value = "";
    searchBox.placeholder = "Search with " + "Frea"
    goHomeButton.addEventListener("click", function() {
      hideSearchResult();
    });
    new Promise((resolve) => {
      showSearchHistory();
      resolve();
    }).then(() => {
      addEventListers();
    });  
  }init();

  searchBox.addEventListener("keypress", async function(event) {
    if (event.keyCode == 13) {
      let searchWords = searchBox.value;
      if (searchWords) {
        searchHistory.push(searchWords);
        if (searchHistory.length > 10) {
          searchHistory.shift();
        }
        Services.prefs.setStringPref("floorp.search.bms.searchHistory", JSON.stringify(searchHistory));
        await searchWithWord(searchWords);
      }
    }
  });

  function showSearchHistory() {
    for (let i = 0; i < searchHistory.length; i++) {
      let searchHistoryItem = document.createElement("div");
      searchHistoryItem.className = "searchHistoryItem";
      searchHistoryItem.textContent = searchHistory[i];
      historyList.insertBefore(searchHistoryItem, historyList.firstChild);
    }
  }

  async function searchWithWord(text) {
   if(Services.prefs.getBoolPref("browser.search.useDefault.enabled")) {
    let engine = await Services.search.getDefault();
    let URL = engine.getSubmission(text, null).uri.spec;
    searchResult.setAttribute("src", URL);
    document.getElementById("searchBox").value = "";
    showSearchResult();
    } else {
      let URL = "https://freasearch.org/search?q=" + text;
      if(searchResult.getAttribute("src") == URL) {
        searchResult.reload();
      } else {
      searchResult.setAttribute("src", URL);
      }
      document.getElementById("searchBox").value = "";
      showSearchResult();
    }
  }

  function showSearchResult() {
    searchResult.setAttribute("hidden", false);
    searchHistoryListContainer.setAttribute("hidden", true);
    reShowSearchHistory(); 
  }

  function hideSearchResult() {
    searchResult.setAttribute("hidden", true);
    searchHistoryListContainer.setAttribute("hidden", false);
  }

  function addEventListers() {
    let searchHistoryItems = document.getElementsByClassName("searchHistoryItem");
    if (searchHistoryItems.length == 0) {
      let noHistoryItem = document.createElement("div");
      noHistoryItem.className = "searchHistoryItem";
      noHistoryItem.textContent = "No search history";
      historyList.appendChild(noHistoryItem);
    }
    for (let i = 0; i < searchHistoryItems.length; i++) {
      searchHistoryItems[i].addEventListener("click", async function(event) {
        let searchWords = event.target.textContent;
        await searchWithWord(searchWords);
      });
    }
  }

  function reShowSearchHistory() {
    let noHistoryItem = document.getElementsByClassName("searchHistoryItem");
    for (let i = 0; i < noHistoryItem.length; i++) {
      noHistoryItem[i].remove();
    } 
    showSearchHistory();
    addEventListers();
  }
});