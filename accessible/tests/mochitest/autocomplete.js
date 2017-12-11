
const nsISupports = Components.interfaces.nsISupports;
const nsIAutoCompleteResult = Components.interfaces.nsIAutoCompleteResult;
const nsIAutoCompleteSearch = Components.interfaces.nsIAutoCompleteSearch;
const nsIFactory = Components.interfaces.nsIFactory;
const nsIUUIDGenerator = Components.interfaces.nsIUUIDGenerator;
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;

var gDefaultAutoCompleteSearch = null;

/**
 * Register 'test-a11y-search' AutoCompleteSearch.
 *
 * @param aValues [in] set of possible results values
 * @param aComments [in] set of possible results descriptions
 */
function initAutoComplete(aValues, aComments) {
  var allResults = new ResultsHeap(aValues, aComments);
  gDefaultAutoCompleteSearch =
    new AutoCompleteSearch("test-a11y-search", allResults);
  registerAutoCompleteSearch(gDefaultAutoCompleteSearch,
                             "Accessibility Test AutoCompleteSearch");
}

/**
 * Unregister 'test-a11y-search' AutoCompleteSearch.
 */
function shutdownAutoComplete() {
  unregisterAutoCompleteSearch(gDefaultAutoCompleteSearch);
  gDefaultAutoCompleteSearch.cid = null;
  gDefaultAutoCompleteSearch = null;
}


/**
 * Register the given AutoCompleteSearch.
 *
 * @param aSearch       [in] AutoCompleteSearch object
 * @param aDescription  [in] description of the search object
 */
function registerAutoCompleteSearch(aSearch, aDescription) {
  var name = "@mozilla.org/autocomplete/search;1?name=" + aSearch.name;

  var uuidGenerator = Components.classes["@mozilla.org/uuid-generator;1"].
    getService(nsIUUIDGenerator);
  var cid = uuidGenerator.generateUUID();

  var componentManager = Components.manager.QueryInterface(nsIComponentRegistrar);
  componentManager.registerFactory(cid, aDescription, name, aSearch);

  // Keep the id on the object so we can unregister later.
  aSearch.cid = cid;
}

/**
 * Unregister the given AutoCompleteSearch.
 */
function unregisterAutoCompleteSearch(aSearch) {
  var componentManager = Components.manager.QueryInterface(nsIComponentRegistrar);
  componentManager.unregisterFactory(aSearch.cid, aSearch);
}


/**
 * A container to keep all possible results of autocomplete search.
 */
function ResultsHeap(aValues, aComments) {
  this.values = aValues;
  this.comments = aComments;
}

ResultsHeap.prototype =
{
  constructor: ResultsHeap,

  /**
   * Return AutoCompleteResult for the given search string.
   */
  getAutoCompleteResultFor(aSearchString) {
    var values = [], comments = [];
    for (var idx = 0; idx < this.values.length; idx++) {
      if (this.values[idx].indexOf(aSearchString) != -1) {
        values.push(this.values[idx]);
        comments.push(this.comments[idx]);
      }
    }
    return new AutoCompleteResult(values, comments);
  }
};


/**
 * nsIAutoCompleteSearch implementation.
 *
 * @param aName       [in] the name of autocomplete search
 * @param aAllResults [in] ResultsHeap object
 */
function AutoCompleteSearch(aName, aAllResults) {
  this.name = aName;
  this.allResults = aAllResults;
}

AutoCompleteSearch.prototype =
{
  constructor: AutoCompleteSearch,

  // nsIAutoCompleteSearch implementation
  startSearch(aSearchString, aSearchParam, aPreviousResult, aListener) {
    var result = this.allResults.getAutoCompleteResultFor(aSearchString);
    aListener.onSearchResult(this, result);
  },

  stopSearch() {},

  // nsISupports implementation
  QueryInterface(iid) {
    if (iid.equals(nsISupports) ||
        iid.equals(nsIFactory) ||
        iid.equals(nsIAutoCompleteSearch))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  // nsIFactory implementation
  createInstance(outer, iid) {
    return this.QueryInterface(iid);
  },

  // Search name. Used by AutoCompleteController.
  name: null,

  // Results heap.
  allResults: null
};


/**
 * nsIAutoCompleteResult implementation.
 */
function AutoCompleteResult(aValues, aComments) {
  this.values = aValues;
  this.comments = aComments;

  if (this.values.length > 0)
    this.searchResult = nsIAutoCompleteResult.RESULT_SUCCESS;
  else
    this.searchResult = nsIAutoCompleteResult.NOMATCH;
}

AutoCompleteResult.prototype =
{
  constructor: AutoCompleteResult,

  searchString: "",
  searchResult: null,

  defaultIndex: 0,

  get matchCount() {
    return this.values.length;
  },

  getValueAt(aIndex) {
    return this.values[aIndex];
  },

  getLabelAt(aIndex) {
    return this.getValueAt(aIndex);
  },

  getCommentAt(aIndex) {
    return this.comments[aIndex];
  },

  getStyleAt(aIndex) {
    return null;
  },

  getImageAt(aIndex) {
    return "";
  },

  getFinalCompleteValueAt(aIndex) {
    return this.getValueAt(aIndex);
  },

  removeValueAt(aRowIndex, aRemoveFromDb) {},

  // nsISupports implementation
  QueryInterface(iid) {
    if (iid.equals(nsISupports) ||
        iid.equals(nsIAutoCompleteResult))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  // Data
  values: null,
  comments: null
};
