/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var gSearchResultsPane = {
  findSelection: null,
  listSearchTooltips: [],
  searchResultsCategory: null,
  searchInput: null,

  init() {
    let controller = this.getSelectionController();
    this.findSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    this.findSelection.setColors("currentColor", "#ffe900", "currentColor", "#540ead");
    this.searchResultsCategory = document.getElementById("category-search-results");

    this.searchInput = document.getElementById("searchInput");
    this.searchInput.hidden = !Services.prefs.getBoolPref("browser.preferences.search");
    if (!this.searchInput.hidden) {
      this.searchInput.addEventListener("command", this);
      this.searchInput.addEventListener("focus", this);
    }
  },

  handleEvent(event) {
    if (event.type === "command") {
      this.searchFunction(event);
    } else if (event.type === "focus") {
      this.initializeCategories();
    }
  },

  /**
   * Check that the passed string matches the filter arguments.
   *
   * @param String str
   *    to search for filter words in.
   * @param String filter
   *    is a string containing all of the words to filter on.
   * @returns boolean
   *    true when match in string else false
   */
  stringMatchesFilters(str, filter) {
    if (!filter || !str) {
      return false;
    }
    let searchStr = str.toLowerCase();
    let filterStrings = filter.toLowerCase().split(/\s+/);
    return !filterStrings.some(f => searchStr.indexOf(f) == -1);
  },

  categoriesInitialized: false,

  /**
   * Will attempt to initialize all uninitialized categories
   */
  initializeCategories() {
    //  Initializing all the JS for all the tabs
    if (!this.categoriesInitialized) {
      this.categoriesInitialized = true;
      // Each element of gCategoryInits is a name
      for (let [/* name */, category] of gCategoryInits) {
        if (!category.inited) {
          category.init();
        }
      }
    }
  },

  /**
   * Finds and returns text nodes within node and all descendants
   * Iterates through all the sibilings of the node object and adds the sibilings
   * to an array if sibiling is a TEXT_NODE else checks the text nodes with in current node
   * Source - http://stackoverflow.com/questions/10730309/find-all-text-nodes-in-html-page
   *
   * @param Node nodeObject
   *    DOM element
   * @returns array of text nodes
   */
  textNodeDescendants(node) {
    if (!node) {
      return [];
    }
    let all = [];
    for (node = node.firstChild; node; node = node.nextSibling) {
      if (node.nodeType === node.TEXT_NODE) {
        all.push(node);
      } else {
        all = all.concat(this.textNodeDescendants(node));
      }
    }
    return all;
  },

  /**
   * This function is used to find words contained within the text nodes.
   * We pass in the textNodes because they contain the text to be highlighted.
   * We pass in the nodeSizes to tell exactly where highlighting need be done.
   * When creating the range for highlighting, if the nodes are section is split
   * by an access key, it is important to have the size of each of the nodes summed.
   * @param Array textNodes
   *    List of DOM elements
   * @param Array nodeSizes
   *    Running size of text nodes. This will contain the same number of elements as textNodes.
   *    The first element is the size of first textNode element.
   *    For any nodes after, they will contain the summation of the nodes thus far in the array.
   *    Example:
   *    textNodes = [[This is ], [a], [n example]]
   *    nodeSizes = [[8], [9], [18]]
   *    This is used to determine the offset when highlighting
   * @param String textSearch
   *    Concatination of textNodes's text content
   *    Example:
   *    textNodes = [[This is ], [a], [n example]]
   *    nodeSizes = "This is an example"
   *    This is used when executing the regular expression
   * @param String searchPhrase
   *    word or words to search for
   * @returns boolean
   *      Returns true when atleast one instance of search phrase is found, otherwise false
   */
  highlightMatches(textNodes, nodeSizes, textSearch, searchPhrase) {
    let indices = [];
    let i = -1;
    while ((i = textSearch.indexOf(searchPhrase, i + 1)) >= 0) {
      indices.push(i);
    }

    // Looping through each spot the searchPhrase is found in the concatenated string
    for (let startValue of indices) {
      let endValue = startValue + searchPhrase.length;
      let startNode = null;
      let endNode = null;
      let nodeStartIndex = null;

      // Determining the start and end node to highlight from
      nodeSizes.forEach(function(lengthNodes, index) {
        // Determining the start node
        if (!startNode && lengthNodes >= startValue) {
          startNode = textNodes[index];
          nodeStartIndex = index;
          // Calculating the offset when found query is not in the first node
          if (index > 0) {
            startValue -= nodeSizes[index - 1];
          }
        }
        // Determining the end node
        if (!endNode && lengthNodes >= endValue) {
          endNode = textNodes[index];
          // Calculating the offset when endNode is different from startNode
          // or when endNode is not the first node
          if (index != nodeStartIndex || index > 0 ) {
            endValue -= nodeSizes[index - 1];
          }
        }
      });
      let range = document.createRange();
      range.setStart(startNode, startValue);
      range.setEnd(endNode, endValue);
      this.findSelection.addRange(range);
    }

    return indices.length > 0;
  },

  getSelectionController() {
    // Yuck. See bug 138068.
    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);

    let controller = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsISelectionDisplay)
                             .QueryInterface(Ci.nsISelectionController);

    return controller;
  },

  get strings() {
    delete this.strings;
    return this.strings = document.getElementById("searchResultBundle");
  },

  /**
   * Shows or hides content according to search input
   *
   * @param String event
   *    to search for filted query in
   */
  searchFunction(event) {
    let query = event.target.value.trim().toLowerCase();
    this.findSelection.removeAllRanges();

    // Remove all search tooltips that were created
    let searchTooltips = Array.from(document.querySelectorAll(".search-tooltip"));
    for (let searchTooltip of searchTooltips) {
      searchTooltip.parentElement.classList.remove("search-tooltip-parent");
      searchTooltip.remove();
    }
    this.listSearchTooltips = [];

    let srHeader = document.getElementById("header-searchResults");

    if (query) {
      // Showing the Search Results Tag
      gotoPref("paneSearchResults");

      this.searchResultsCategory.hidden = false;

      let resultsFound = false;

      // Building the range for highlighted areas
      let rootPreferencesChildren = document
        .querySelectorAll("#mainPrefPane > *:not([data-hidden-from-search])");

      // Showing all the children to bind JS, Access Keys, etc
      for (let i = 0; i < rootPreferencesChildren.length; i++) {
        rootPreferencesChildren[i].hidden = false;
      }

      // Showing or Hiding specific section depending on if words in query are found
      for (let i = 0; i < rootPreferencesChildren.length; i++) {
        if (rootPreferencesChildren[i].className != "header" &&
            rootPreferencesChildren[i].className != "no-results-message" &&
            this.searchWithinNode(rootPreferencesChildren[i], query)) {
          rootPreferencesChildren[i].hidden = false;
          resultsFound = true;
        } else {
          rootPreferencesChildren[i].hidden = true;
        }
      }
      // It hides Search Results header so turning it on
      srHeader.hidden = false;

      if (!resultsFound) {
        let noResultsEl = document.querySelector(".no-results-message");
        noResultsEl.hidden = false;

        let strings = this.strings;

        document.getElementById("sorry-message").textContent = AppConstants.platform == "win" ?
          strings.getFormattedString("searchResults.sorryMessageWin", [query]) :
          strings.getFormattedString("searchResults.sorryMessageUnix", [query]);
        let brandName = document.getElementById("bundleBrand").getString("brandShortName");
        document.getElementById("need-help").innerHTML =
          strings.getFormattedString("searchResults.needHelp", [brandName]);
      } else {
        // Creating tooltips for all the instances found
        for (let node of this.listSearchTooltips) {
          this.createSearchTooltip(node, query);
        }
      }
    } else {
      this.searchResultsCategory.hidden = true;
      document.getElementById("sorry-message").textContent = "";
      // Going back to General when cleared
      gotoPref("paneGeneral");
    }
  },

  /**
   * Finding leaf nodes and checking their content for words to search,
   * It is a recursive function
   *
   * @param Node nodeObject
   *    DOM Element
   * @param String searchPhrase
   * @returns boolean
   *    Returns true when found in at least one childNode, false otherwise
   */
  searchWithinNode(nodeObject, searchPhrase) {
    let matchesFound = false;
    if (nodeObject.childElementCount == 0 || nodeObject.tagName == "menulist") {
      let simpleTextNodes = this.textNodeDescendants(nodeObject);

      for (let node of simpleTextNodes) {
        let result = this.highlightMatches([node], [node.length], node.textContent.toLowerCase(), searchPhrase);
        matchesFound = matchesFound || result;
      }

      // Collecting data from boxObject
      let nodeSizes = [];
      let allNodeText = "";
      let runningSize = 0;
      let accessKeyTextNodes = this.textNodeDescendants(nodeObject.boxObject);

      for (let node of accessKeyTextNodes) {
        runningSize += node.textContent.length;
        allNodeText += node.textContent;
        nodeSizes.push(runningSize);
      }

      // Access key are presented
      let complexTextNodesResult = this.highlightMatches(accessKeyTextNodes, nodeSizes, allNodeText.toLowerCase(), searchPhrase);

      // Searching some elements, such as xul:button, have a 'label' attribute that contains the user-visible text.
      let labelResult = this.stringMatchesFilters(nodeObject.getAttribute("label"), searchPhrase);

      // Creating tooltips for buttons
      if (labelResult && nodeObject.tagName === "button") {
        this.listSearchTooltips.push(nodeObject);
      }

      // Searching some elements, such as xul:label, store their user-visible text in a "value" attribute.
      let valueResult = this.stringMatchesFilters(nodeObject.getAttribute("value"), searchPhrase);

      // Creating tooltips for buttons
      if (valueResult && nodeObject.tagName === "button") {
        this.listSearchTooltips.push(nodeObject);
      }

      // Searching some elements, such as xul:button, buttons to open subdialogs.
      let keywordsResult = this.stringMatchesFilters(nodeObject.getAttribute("searchkeywords"), searchPhrase);

      // Creating tooltips for buttons
      if (keywordsResult && nodeObject.tagName === "button") {
        this.listSearchTooltips.push(nodeObject);
      }

      if (nodeObject.tagName == "button" && (labelResult || valueResult || keywordsResult)) {
        nodeObject.setAttribute("highlightable", "true");
      }

      matchesFound = matchesFound || complexTextNodesResult || labelResult || valueResult || keywordsResult;
    }

    for (let i = 0; i < nodeObject.childNodes.length; i++) {
      // Search only if child node is not hidden
      if (!nodeObject.childNodes[i].hidden && nodeObject.getAttribute("data-hidden-from-search") !== "true") {
        let result = this.searchWithinNode(nodeObject.childNodes[i], searchPhrase);
        // Creating tooltips for menulist element
        if (result && nodeObject.tagName === "menulist") {
          this.listSearchTooltips.push(nodeObject);
        }
        matchesFound = matchesFound || result;
      }
    }
    return matchesFound;
  },

  /**
   * Inserting a div structure infront of the DOM element matched textContent.
   * Then calculation the offsets to position the tooltip in the correct place.
   *
   * @param Node currentNode
   *    DOM Element
   * @param String query
   *    Word or words that are being searched for
   */
  createSearchTooltip(currentNode, query) {
    let searchTooltip = document.createElement("span");
    searchTooltip.setAttribute("class", "search-tooltip");
    searchTooltip.textContent = query;

    currentNode.parentElement.classList.add("search-tooltip-parent");
    currentNode.parentElement.appendChild(searchTooltip);

    // In order to get the up-to-date position of each of the nodes that we're
    // putting tooltips on, we have to flush layout intentionally, and that
    // this is the result of a XUL limitation (bug 1363730).
    let anchorRect = currentNode.getBoundingClientRect();
    let tooltipRect = searchTooltip.getBoundingClientRect();
    let parentRect = currentNode.parentElement.getBoundingClientRect();

    let offSet = (anchorRect.width / 2) - (tooltipRect.width / 2);
    let relativeOffset = anchorRect.left - parentRect.left;
    offSet += relativeOffset > 0 ? relativeOffset : 0;

    searchTooltip.style.setProperty("left", `${offSet}px`);
  }
}
