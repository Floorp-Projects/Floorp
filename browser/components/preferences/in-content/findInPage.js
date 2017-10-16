/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var gSearchResultsPane = {
  listSearchTooltips: new Set(),
  listSearchMenuitemIndicators: new Set(),
  searchInput: null,
  inited: false,

  init() {
    if (this.inited) {
      return;
    }
    this.inited = true;
    this.searchInput = document.getElementById("searchInput");
    this.searchInput.hidden = !Services.prefs.getBoolPref("browser.preferences.search");
    if (!this.searchInput.hidden) {
      this.searchInput.addEventListener("input", this);
      this.searchInput.addEventListener("command", this);
      window.addEventListener("DOMContentLoaded", () => {
        this.searchInput.focus();
      });
      // Initialize other panes in an idle callback.
      window.requestIdleCallback(() => this.initializeCategories());
    }
    let strings = this.strings;
    this.searchInput.placeholder = AppConstants.platform == "win" ?
      strings.getString("searchInput.labelWin") :
      strings.getString("searchInput.labelUnix");
  },

  handleEvent(event) {
    // Ensure categories are initialized if idle callback didn't run sooo enough.
    this.initializeCategories();
    this.searchFunction(event);
  },

  /**
   * Check that the text content contains the query string.
   *
   * @param String content
   *    the text content to be searched
   * @param String query
   *    the query string
   * @returns boolean
   *    true when the text content contains the query string else false
   */
  queryMatchesContent(content, query) {
    if (!content || !query) {
      return false;
    }
    return content.toLowerCase().includes(query.toLowerCase());
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
    if (!searchPhrase) {
      return false;
    }

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
      for (let index = 0; index < nodeSizes.length; index++) {
        let lengthNodes = nodeSizes[index];
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
      }
      let range = document.createRange();
      range.setStart(startNode, startValue);
      range.setEnd(endNode, endValue);
      this.getFindSelection(startNode.ownerGlobal).addRange(range);
    }

    return indices.length > 0;
  },

  /**
   * Get the selection instance from given window
   *
   * @param Object win
   *   The window object points to frame's window
   */
  getFindSelection(win) {
    // Yuck. See bug 138068.
    let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebNavigation)
                      .QueryInterface(Ci.nsIDocShell);

    let controller = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsISelectionDisplay)
                              .QueryInterface(Ci.nsISelectionController);

    let selection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    selection.setColors("currentColor", "#ffe900", "currentColor", "#003eaa");

    return selection;
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
  async searchFunction(event) {
    let query = event.target.value.trim().toLowerCase();
    if (this.query == query) {
      return;
    }

    let subQuery = this.query && query.indexOf(this.query) !== -1;
    this.query = query;

    this.getFindSelection(window).removeAllRanges();
    this.removeAllSearchTooltips();
    this.removeAllSearchMenuitemIndicators();

    // Clear telemetry request if user types very frequently.
    if (this.telemetryTimer) {
      clearTimeout(this.telemetryTimer);
    }

    let srHeader = document.getElementById("header-searchResults");

    if (this.query) {
      // Showing the Search Results Tag
      gotoPref("paneSearchResults");

      let resultsFound = false;

      // Building the range for highlighted areas
      let rootPreferencesChildren = document
        .querySelectorAll("#mainPrefPane > *:not([data-hidden-from-search])");

      // Show all second level headers in search result
      for (let element of document.querySelectorAll("caption.search-header")) {
        element.hidden = false;
      }

      if (subQuery) {
        // Since the previous query is a subset of the current query,
        // there is no need to check elements that is hidden already.
        rootPreferencesChildren =
          Array.prototype.filter.call(rootPreferencesChildren, el => !el.hidden);
      }

      // Mark all the children to check be visible to bind JS, Access Keys, etc,
      // but don't really show them by setting their visibility to hidden in CSS.
      for (let i = 0; i < rootPreferencesChildren.length; i++) {
        rootPreferencesChildren[i].hidden = false;
        rootPreferencesChildren[i].classList.add("visually-hidden");
      }

      let ts = performance.now();
      let FRAME_THRESHOLD = 1000 / 60;

      // Showing or Hiding specific section depending on if words in query are found
      for (let i = 0; i < rootPreferencesChildren.length; i++) {
        if (performance.now() - ts > FRAME_THRESHOLD) {
          // Creating tooltips for all the instances found
          for (let anchorNode of this.listSearchTooltips) {
            this.createSearchTooltip(anchorNode, this.query);
          }
          // It hides Search Results header so turning it on
          srHeader.hidden = false;
          srHeader.classList.remove("visually-hidden");
          ts = await new Promise(resolve => window.requestAnimationFrame(resolve));
          if (query !== this.query) {
            return;
          }
        }

        rootPreferencesChildren[i].classList.remove("visually-hidden");
        if (!rootPreferencesChildren[i].classList.contains("header") &&
            !rootPreferencesChildren[i].classList.contains("subcategory") &&
            !rootPreferencesChildren[i].classList.contains("no-results-message") &&
            this.searchWithinNode(rootPreferencesChildren[i], this.query)) {
          rootPreferencesChildren[i].hidden = false;
          resultsFound = true;
        } else {
          rootPreferencesChildren[i].hidden = true;
        }
      }
      // It hides Search Results header so turning it on
      srHeader.hidden = false;
      srHeader.classList.remove("visually-hidden");

      if (!resultsFound) {
        let noResultsEl = document.querySelector(".no-results-message");
        noResultsEl.setAttribute("query", this.query);
        noResultsEl.hidden = false;

        let strings = this.strings;

        document.getElementById("sorry-message").textContent = AppConstants.platform == "win" ?
          strings.getFormattedString("searchResults.sorryMessageWin", [this.query]) :
          strings.getFormattedString("searchResults.sorryMessageUnix", [this.query]);
        let helpUrl = Services.urlFormatter.formatURLPref("app.support.baseURL") + "preferences";
        let brandName = document.getElementById("bundleBrand").getString("brandShortName");
        // eslint-disable-next-line no-unsanitized/property
        document.getElementById("need-help").innerHTML =
          strings.getFormattedString("searchResults.needHelp2", [helpUrl, brandName]);
      } else {
        // Creating tooltips for all the instances found
        for (let anchorNode of this.listSearchTooltips) {
          this.createSearchTooltip(anchorNode, this.query);
        }

        // Implant search telemetry probe after user stops typing for a while
        if (this.query.length >= 2) {
          this.telemetryTimer = setTimeout(() => {
            Services.telemetry.keyedScalarAdd("preferences.search_query", this.query, 1);
          }, 1000);
        }
      }
    } else {
      document.getElementById("sorry-message").textContent = "";
      // Going back to General when cleared
      gotoPref("paneGeneral");

      // Hide some special second level headers in normal view
      for (let element of document.querySelectorAll("caption.search-header")) {
        element.hidden = true;
      }
    }

    window.dispatchEvent(new CustomEvent("PreferencesSearchCompleted", { detail: query }));
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
    if (nodeObject.childElementCount == 0 ||
        nodeObject.tagName == "label" ||
        nodeObject.tagName == "description" ||
        nodeObject.tagName == "menulist") {
      let simpleTextNodes = this.textNodeDescendants(nodeObject);
      for (let node of simpleTextNodes) {
        let result = this.highlightMatches([node], [node.length], node.textContent.toLowerCase(), searchPhrase);
        matchesFound = matchesFound || result;
      }

      // Collecting data from boxObject / label / description
      let nodeSizes = [];
      let allNodeText = "";
      let runningSize = 0;
      let accessKeyTextNodes = this.textNodeDescendants(nodeObject.boxObject);

      if (nodeObject.tagName == "label" || nodeObject.tagName == "description") {
        accessKeyTextNodes.push(...this.textNodeDescendants(nodeObject));
      }

      for (let node of accessKeyTextNodes) {
        runningSize += node.textContent.length;
        allNodeText += node.textContent;
        nodeSizes.push(runningSize);
      }

      // Access key are presented
      let complexTextNodesResult = this.highlightMatches(accessKeyTextNodes, nodeSizes, allNodeText.toLowerCase(), searchPhrase);

      // Searching some elements, such as xul:button, have a 'label' attribute that contains the user-visible text.
      let labelResult = this.queryMatchesContent(nodeObject.getAttribute("label"), searchPhrase);

      // Searching some elements, such as xul:label, store their user-visible text in a "value" attribute.
      // Value will be skipped for menuitem since value in menuitem could represent index number to distinct each item.
      let valueResult = nodeObject.tagName !== "menuitem" ?
        this.queryMatchesContent(nodeObject.getAttribute("value"), searchPhrase) : false;

      // Searching some elements, such as xul:button, buttons to open subdialogs.
      let keywordsResult = this.queryMatchesContent(nodeObject.getAttribute("searchkeywords"), searchPhrase);

      // Creating tooltips for buttons
      if (keywordsResult && (nodeObject.tagName === "button" || nodeObject.tagName == "menulist")) {
        this.listSearchTooltips.add(nodeObject);
      }

      if (keywordsResult && nodeObject.tagName === "menuitem") {
        nodeObject.setAttribute("indicator", "true");
        this.listSearchMenuitemIndicators.add(nodeObject);
        let menulist = nodeObject.closest("menulist");

        menulist.setAttribute("indicator", "true");
        this.listSearchMenuitemIndicators.add(menulist);
      }

      if ((nodeObject.tagName == "button" ||
           nodeObject.tagName == "menulist" ||
           nodeObject.tagName == "menuitem") &&
           (labelResult || valueResult || keywordsResult)) {
        nodeObject.setAttribute("highlightable", "true");
      }

      matchesFound = matchesFound || complexTextNodesResult || labelResult || valueResult || keywordsResult;
    }

    // Should not search unselected child nodes of a <xul:deck> element
    // except the "historyPane" <xul:deck> element.
    if (nodeObject.tagName == "deck" && nodeObject.id != "historyPane") {
      let index = nodeObject.selectedIndex;
      if (index != -1) {
        let result = this.searchChildNodeIfVisible(nodeObject, index, searchPhrase);
        matchesFound = matchesFound || result;
      }
    } else {
      for (let i = 0; i < nodeObject.childNodes.length; i++) {
        let result = this.searchChildNodeIfVisible(nodeObject, i, searchPhrase);
        matchesFound = matchesFound || result;
      }
    }
    return matchesFound;
  },

  /**
   * Search for a phrase within a child node if it is visible.
   *
   * @param Node nodeObject
   *    The parent DOM Element
   * @param Number index
   *    The index for the childNode
   * @param String searchPhrase
   * @returns boolean
   *    Returns true when found the specific childNode, false otherwise
   */
  searchChildNodeIfVisible(nodeObject, index, searchPhrase) {
    let result = false;
    if (!nodeObject.childNodes[index].hidden && nodeObject.getAttribute("data-hidden-from-search") !== "true") {
      result = this.searchWithinNode(nodeObject.childNodes[index], searchPhrase);
      // Creating tooltips for menulist element
      if (result && nodeObject.tagName === "menulist") {
        this.listSearchTooltips.add(nodeObject);
      }
    }
    return result;
  },

  /**
   * Inserting a div structure infront of the DOM element matched textContent.
   * Then calculation the offsets to position the tooltip in the correct place.
   *
   * @param Node anchorNode
   *    DOM Element
   * @param String query
   *    Word or words that are being searched for
   */
  createSearchTooltip(anchorNode, query) {
    if (anchorNode.tooltipNode) {
      return;
    }
    let searchTooltip = anchorNode.ownerDocument.createElement("span");
    let searchTooltipText = anchorNode.ownerDocument.createElement("span");
    searchTooltip.setAttribute("class", "search-tooltip");
    searchTooltipText.textContent = query;
    searchTooltip.appendChild(searchTooltipText);

    // Set tooltipNode property to track corresponded tooltip node.
    anchorNode.tooltipNode = searchTooltip;
    anchorNode.parentElement.classList.add("search-tooltip-parent");
    anchorNode.parentElement.appendChild(searchTooltip);

    this.calculateTooltipPosition(anchorNode);
  },

  calculateTooltipPosition(anchorNode) {
    let searchTooltip = anchorNode.tooltipNode;
    // In order to get the up-to-date position of each of the nodes that we're
    // putting tooltips on, we have to flush layout intentionally, and that
    // this is the result of a XUL limitation (bug 1363730).
    let tooltipRect = searchTooltip.getBoundingClientRect();
    searchTooltip.style.setProperty("left", `calc(50% - ${(tooltipRect.width / 2)}px)`);
  },

  /**
   * Remove all search tooltips that were created.
   */
  removeAllSearchTooltips() {
    let searchTooltips = Array.from(document.querySelectorAll(".search-tooltip"));
    for (let searchTooltip of searchTooltips) {
      searchTooltip.parentElement.classList.remove("search-tooltip-parent");
      searchTooltip.remove();
    }
    for (let anchorNode of this.listSearchTooltips) {
      anchorNode.tooltipNode.remove();
      anchorNode.tooltipNode = null;
    }
    this.listSearchTooltips.clear();
  },

  /**
   * Remove all indicators on menuitem.
   */
  removeAllSearchMenuitemIndicators() {
    for (let node of this.listSearchMenuitemIndicators) {
      node.removeAttribute("indicator");
    }
    this.listSearchMenuitemIndicators.clear();
  }
};
