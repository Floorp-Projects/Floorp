/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Suggest Autocomplete Implementation for Firefox.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *   Mike Connor <mconnor@mozilla.com>
 *   Joe Hughes  <joe@retrovirus.com>
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

/**
 * Metadata describing the Web Search suggest mode
 */
const SEARCH_SUGGEST_CONTRACTID =
  "@mozilla.org/autocomplete/search;1?name=search-autocomplete";
const SEARCH_SUGGEST_CLASSNAME = "Remote Search Suggestions";
const SEARCH_SUGGEST_CLASSID =
  Components.ID("{aa892eb4-ffbf-477d-9f9a-06c995ae9f27}");

const SEARCH_BUNDLE = "chrome://browser/locale/search.properties";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const HTTP_OK                    = 200;
const HTTP_INTERNAL_SERVER_ERROR = 500;
const HTTP_BAD_GATEWAY           = 502;
const HTTP_SERVICE_UNAVAILABLE   = 503;

/**
 * SuggestAutoCompleteResult contains the results returned by the Suggest
 * service - it implements nsIAutoCompleteResult and is used by the auto-
 * complete controller to populate the front end.
 * @constructor
 */
function SuggestAutoCompleteResult(searchString,
                                   searchResult,
                                   defaultIndex,
                                   errorDescription,
                                   results,
                                   comments) {
  this._searchString = searchString;
  this._searchResult = searchResult;
  this._defaultIndex = defaultIndex;
  this._errorDescription = errorDescription;
  this._results = results;
  this._comments = comments;
}
SuggestAutoCompleteResult.prototype = {
  /**
   * The user's query string
   * @private
   */
  _searchString: "",

  /**
   * The result code of this result object, see |get searchResult| for possible
   * values.
   * @private
   */
  _searchResult: 0,

  /**
   * The default item that should be entered if none is selected
   * @private
   */
  _defaultIndex: 0,

  /**
   * The reason the search failed
   * @private
   */
  _errorDescription: "",

  /**
   * The list of URLs returned by the Suggest Service
   * @private
   */
  _results: [],

  /**
   * The list of Comments (number of results - or page titles) returned by the
   * Suggest Service.
   * @private
   */
  _comments: [],

  /**
   * @return the user's query string
   */
  get searchString() {
    return this._searchString;
  },

  /**
   * @return the result code of this result object, either:
   *         RESULT_IGNORED   (invalid searchString)
   *         RESULT_FAILURE   (failure)
   *         RESULT_NOMATCH   (no matches found)
   *         RESULT_SUCCESS   (matches found)
   */
  get searchResult() {
    return this._searchResult;
  },

  /**
   * @return the default item that should be entered if none is selected
   */
  get defaultIndex() {
    return this._defaultIndex;
  },

  /**
   * @return the reason the search failed
   */
  get errorDescription() {
    return this._errorDescription;
  },

  /**
   * @return the number of results
   */
  get matchCount() {
    return this._results.length;
  },

  /**
   * Retrieves a result
   * @param  index    the index of the result requested
   * @return          the result at the specified index
   */
  getValueAt: function(index) {
    return this._results[index];
  },

  /**
   * Retrieves a comment (metadata instance)
   * @param  index    the index of the comment requested
   * @return          the comment at the specified index
   */
  getCommentAt: function(index) {
    return this._comments[index];
  },

  /**
   * Retrieves a style hint specific to a particular index.
   * @param  index    the index of the style hint requested
   * @return          the style hint at the specified index
   */
  getStyleAt: function(index) {
    if (!this._comments[index])
      return null;  // not a category label, so no special styling

    if (index == 0)
      return "suggestfirst";  // category label on first line of results

    return "suggesthint";   // category label on any other line of results
  },

  /**
   * Removes a result from the resultset
   * @param  index    the index of the result to remove
   */
  removeValueAt: function(index, removeFromDatabase) {
    this._results.splice(index, 1);
    this._comments.splice(index, 1);
  },

  /**
   * Part of nsISupports implementation.
   * @param   iid     requested interface identifier
   * @return  this object (XPConnect handles the magic of telling the caller that
   *                       we're the type it requested)
   */
  QueryInterface: function(iid) {
    if (!iid.equals(Ci.nsIAutoCompleteResult) &&
        !iid.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * SuggestAutoComplete is a base class that implements nsIAutoCompleteSearch
 * and can collect results for a given search by using the search URL supplied
 * by the subclass. We do it this way since the AutoCompleteController in
 * Mozilla requires a unique XPCOM Service for every search provider, even if
 * the logic for two providers is identical.
 * @constructor
 */
function SuggestAutoComplete() {}
SuggestAutoComplete.prototype = {

  /**
   * this._strings is the string bundle for message internationalization.
   */
  get _strings() {
    if (!this.__strings) {
      var sbs = Cc["@mozilla.org/intl/stringbundle;1"].
                getService(Ci.nsIStringBundleService);

      this.__strings = sbs.createBundle(SEARCH_BUNDLE);
    }
    return this.__strings;
  },
  __strings: null,

  /*************************************************************************
   * Server request backoff implementation fields below
   * These allow us to throttle requests if the server is getting hammered.
   **************************************************************************/

  /**
   * This is an array that contains the timestamps (in unixtime) of
   * the last few backoff-triggering errors.
   */
  _serverErrorLog: [],

  /**
   * If we receive this number of backoff errors within the amount of time
   * specified by _serverErrorPeriod, then we initiate backoff.
   */
  _maxErrorsBeforeBackoff: 3,

  /**
   * If we receive enough consecutive errors (where "enough" is defined by
   * _maxErrorsBeforeBackoff above) within this time period,
   * we trigger the backoff behavior.
   */
  _serverErrorPeriod: 600000,  // 10 minutes in milliseconds

  /**
   * If we get another backoff error immediately after timeout, we increase the
   * backoff to (2 x old period) + this value.
   */
  _serverErrorTimeoutIncrement: 600000,  // 10 minutes in milliseconds

  /**
   * The current amount of time to wait before trying a server request
   * after receiving a backoff error.
   */
  _serverErrorTimeout: 0,

  /**
   * Time (in unixtime) after which we're allowed to try requesting again.
   */
  _nextRequestTime: 0,

  /**
   * The last URI we requested against (so that we can tell if the
   * user switched engines).
   */
  _serverErrorURI: null,

  /**
   * The XMLHttpRequest object.
   * @private
   */
  _request: null,

  /**
   * The object implementing nsIAutoCompleteObserver that we notify when
   * we have found results
   * @private
   */
  _listener: null,

  /**
   * If this is true, we'll integrate form history results with the
   * suggest results.
   */
  _includeFormHistory: true,

  /**
   * True if a request for remote suggestions was sent. This is used to
   * differentiate between the "_request is null because the request has
   * already returned a result" and "_request is null because no request was
   * sent" cases.
   */
  _sentSuggestRequest: false,

  /**
   * This is the callback for the suggest timeout timer.  If this gets
   * called, it means that we've given up on receiving a reply from the
   * search engine's suggestion server in a timely manner.
   */
  notify: function SAC_notify(timer) {
    // make sure we're still waiting for this response before sending
    if ((timer != this._formHistoryTimer) || !this._listener)
      return;

    this._listener.onSearchResult(this, this._formHistoryResult);
    this._reset();
  },

  /**
   * This determines how long (in ms) we should wait before giving up on
   * the suggestions and just showing local form history results.
   */
  _suggestionTimeout: 500,

  /**
   * This is the callback for that the form history service uses to
   * send us results.
   */
  onSearchResult: function SAC_onSearchResult(search, result) {
    this._formHistoryResult = result;

    if (this._request) {
      // We still have a pending request, wait a bit to give it a chance to
      // finish.
      this._formHistoryTimer = Cc["@mozilla.org/timer;1"].
                               createInstance(Ci.nsITimer);
      this._formHistoryTimer.initWithCallback(this, this._suggestionTimeout,
                                              Ci.nsITimer.TYPE_ONE_SHOT);
    } else if (!this._sentSuggestRequest) {
      // We didn't send a request, so just send back the form history results.
      this._listener.onSearchResult(this, this._formHistoryResult);
    }
  },

  /**
   * Autocomplete results from the form history service get stored here.
   */
  _formHistoryResult: null,

  /**
   * This holds the suggest server timeout timer, if applicable.
   */
  _formHistoryTimer: null,

  /**
   * This clears all the per-request state.
   */
  _reset: function SAC_reset() {
    if (this._formHistoryTimer)
      this._formHistoryTimer.cancel();
    this._formHistoryTimer = null;
    this._formHistoryResult = null;
    this._listener = null;
    this._request = null;
  },

  /**
   * This sends an autocompletion request to the form history service,
   * which will call onSearchResults with the results of the query.
   */
  _startHistorySearch: function SAC_SHSearch(searchString, searchParam, previousResult) {
    var formHistory =
      Cc["@mozilla.org/autocomplete/search;1?name=form-history"].
      createInstance(Ci.nsIAutoCompleteSearch);
    formHistory.startSearch(searchString, searchParam, previousResult, this);
  },

  /**
   * Makes a note of the fact that we've recieved a backoff-triggering
   * response, so that we can adjust the backoff behavior appropriately.
   */
  _noteServerError: function SAC__noteServeError() {
    var currentTime = Date.now();

    this._serverErrorLog.push(currentTime);
    if (this._serverErrorLog.length > this._maxErrorsBeforeBackoff)
      this._serverErrorLog.shift();

    if ((this._serverErrorLog.length == this._maxErrorsBeforeBackoff) &&
        ((currentTime - this._serverErrorLog[0]) < this._serverErrorPeriod)) {
      // increase timeout, and then don't request until timeout is over
      this._serverErrorTimeout = (this._serverErrorTimeout * 2) +
                                 this._serverErrorTimeoutIncrement;
      this._nextRequestTime = currentTime + this._serverErrorTimeout;
    }
  },

  /**
   * Resets the backoff behavior; called when we get a successful response.
   */
  _clearServerErrors: function SAC__clearServerErrors() {
    this._serverErrorLog = [];
    this._serverErrorTimeout = 0;
    this._nextRequestTime = 0;
  },

  /**
   * This checks whether we should send a server request (i.e. we're not
   * in a error-triggered backoff period.
   *
   * @private
   */
  _okToRequest: function SAC__okToRequest() {
    return Date.now() > this._nextRequestTime;
  },

  /**
   * This checks to see if the new search suggestion URI is different
   * from the previous one, and if so clears any error state that might
   * have accumulated for the old engine/URI.
   *
   * @param uri The URI that the suggestion request would be sent to.
   * @private
   */
  _checkForEngineSwitch: function SAC__checkForEngineSwitch(uri) {
    if (uri == this._serverErrorURI)
      return;

    // must've switched search providers, clear old errors
    this._serverErrorURI = uri;
    this._clearServerErrors();
  },

  /**
   * This returns true if the status code of the HTTP response
   * represents a backoff-triggering error.
   *
   * @param status  The status code from the HTTP response
   * @private
   */
  _isBackoffError: function SAC__isBackoffError(status) {
    return ((status == HTTP_INTERNAL_SERVER_ERROR) ||
            (status == HTTP_BAD_GATEWAY) ||
            (status == HTTP_SERVICE_UNAVAILABLE));
  },

  /**
   * Called when the 'readyState' of the XMLHttpRequest changes. We only care
   * about state 4 (COMPLETED) - handle the response data.
   * @private
   */
  onReadyStateChange: function() {
    // xxx use the real const here
    if (!this._request || this._request.readyState != 4)
      return;

    try {
      var status = this._request.status;
    } catch (e) {
      // The XML HttpRequest can throw NS_ERROR_NOT_AVAILABLE.
      return;
    }

    if (this._isBackoffError(status)) {
      this._noteServerError();
      return;
    }

    var responseText = this._request.responseText;
    if (status != HTTP_OK || responseText == "")
      return;

    this._clearServerErrors();

    var searchString, results, queryURLs;
    var searchService = Cc["@mozilla.org/browser/search-service;1"].
                        getService(Ci.nsIBrowserSearchService);
    var sandboxHost = searchService.currentEngine.suggestionURI.prePath;
    var sandbox = new Components.utils.Sandbox(sandboxHost);
    var results2 = Components.utils.evalInSandbox(responseText, sandbox);

    if (results2[0]) {
      searchString = results2[0] ? results2[0] : "";
      results = results2[1] ? results2[1] : [];
    } else {
      // this is backwards compat code for Google Suggest, to be removed
      // once they shift to the new format
      // The responseText is formatted like so:
      // searchString\n"r1","r2","r3"\n"c1","c2","c3"\n"p1","p2","p3"
      // ... where all values are escaped:
      //  rX = result  (search term or URL)
      //  cX = comment (number of results or page title)
      //  pX = prefix

      // Note that right now we're using the "comment" column of the
      // autocomplete dropdown to indicate where the suggestions
      // begin, so we're discarding the comments from the server.
      var parts = responseText.split("\n");
      results = parts[1] ? parts[1].split(",") : [];
      for (var i = 0; i < results.length; ++i) {
        results[i] = unescape(results[i]);
        results[i] = results[i].substr(1, results[i].length - 2);
      }
    }

    var comments = [];  // "comments" column values for suggestions
    var historyResults = [];
    var historyComments = [];

    // If form history is enabled and has results, add them to the list.
    if (this._includeFormHistory && this._formHistoryResult &&
        (this._formHistoryResult.searchResult ==
         Ci.nsIAutoCompleteResult.RESULT_SUCCESS)) {
      for (var i = 0; i < this._formHistoryResult.matchCount; ++i) {
        var term = this._formHistoryResult.getValueAt(i);

        // we don't want things to appear in both history and suggestions
        var dupIndex = results.indexOf(term);
        if (dupIndex != -1)
          results.splice(dupIndex, 1);

        historyResults.push(term);
        historyComments.push("");
      }

      this._formHistoryResult = null;
    }

    // fill out the comment column for the suggestions
    for (var i = 0; i < results.length; ++i)
      comments.push("");

    // if we have any suggestions, put a label at the top
    if (comments.length > 0)
      comments[0] = this._strings.GetStringFromName("suggestion_label");

    // now put the history results above the suggestions
    var finalResults = historyResults.concat(results);
    var finalComments = historyComments.concat(comments);

    // Notify the FE of our new results
    this.onResultsReady(searchString, finalResults, finalComments);

    // Reset our state for next time.
    this._reset();
  },

  /**
   * Notifies the front end of new results.
   * @param searchString  the user's query string
   * @param results       an array of results to the search
   * @param comments      an array of metadata corresponding to the results
   * @private
   */
  onResultsReady: function(searchString, results, comments) {
    if (this._listener) {
      var result = new SuggestAutoCompleteResult(
          searchString,
          Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
          0,
          "",
          results,
          comments);
      this._listener.onSearchResult(this, result);
    }
  },

  /**
   * Initiates the search result gathering process. Part of
   * nsIAutoCompleteSearch implementation.
   *
   * @param searchString    the user's query string
   * @param searchParam     unused, "an extra parameter"; even though
   *                        this parameter and the next are unused, pass
   *                        them through in case the form history
   *                        service wants them
   * @param previousResult  unused, a client-cached store of the previous
   *                        generated resultset for faster searching.
   * @param listener        object implementing nsIAutoCompleteObserver which
   *                        we notify when results are ready.
   */
  startSearch: function(searchString, searchParam, previousResult, listener) {
    var searchService = Cc["@mozilla.org/browser/search-service;1"].
                        getService(Ci.nsIBrowserSearchService);

    // If there's an existing request, stop it. There is no smart filtering
    // here as there is when looking through history/form data because the
    // result set returned by the server is different for every typed value -
    // "ocean breathes" does not return a subset of the results returned for
    // "ocean", for example. This does nothing if there is no current request.
    this.stopSearch();

    this._listener = listener;

    var suggestionURI = searchService.currentEngine.suggestionURI;
    this._checkForEngineSwitch(suggestionURI);

    if (!searchString || !suggestionURI || !this._okToRequest()) {
      // We have an empty search string (user pressed down arrow to see
      // history), or the current engine has no suggest functionality,
      // or we're in backoff mode; so just use local history.
      this._sentSuggestRequest = false;
      this._startHistorySearch(searchString, searchParam, previousResult);
      return;
    }

    var suggestionURL = suggestionURI.spec;
    // Actually do the search
    this._request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    this._request.open("GET", suggestionURL + encodeURIComponent(searchString), true);

    var self = this;
    function onReadyStateChange() {
      self.onReadyStateChange();
    }
    this._request.onreadystatechange = onReadyStateChange;
    this._request.send(null);

    if (this._includeFormHistory) {
      this._sentSuggestRequest = true;
      this._startHistorySearch(searchString, searchParam, previousResult);
    }
  },

  /**
   * Ends the search result gathering process. Part of nsIAutoCompleteSearch
   * implementation.
   */
  stopSearch: function() {
    if (this._request) {
      this._request.abort();
      this._reset();
    }
  },

  /**
   * Part of nsISupports implementation.
   * @param   iid     requested interface identifier
   * @return  this object (XPConnect handles the magic of telling the caller that
   *                       we're the type it requested)
   */
  QueryInterface: function(iid) {
    if (!iid.equals(Ci.nsIAutoCompleteSearch) &&
        !iid.equals(Ci.nsIAutoCompleteObserver) &&
        !iid.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * SearchSuggestAutoComplete is a service implementation that handles suggest
 * results specific to web searches.
 * @constructor
 */
function SearchSuggestAutoComplete() {
}
SearchSuggestAutoComplete.prototype = {
  __proto__: SuggestAutoComplete.prototype,
  serviceURL: ""
};

var gModule = {
  /**
   * Registers all the components supplied by this module. Part of nsIModule
   * implementation.
   * @param componentManager  the XPCOM component manager
   * @param location          the location of the module on disk
   * @param loaderString      opaque loader specific string
   * @param type              loader type being used to load this module
   */
  registerSelf: function(componentManager, location, loaderString, type) {
    if (this._firstTime) {
      this._firstTime = false;
      throw Cr.NS_ERROR_FACTORY_REGISTER_AGAIN;
    }
    componentManager =
      componentManager.QueryInterface(Ci.nsIComponentRegistrar);

    for (var key in this.objects) {
      var obj = this.objects[key];
      componentManager.registerFactoryLocation(obj.CID, obj.className, obj.contractID,
                                               location, loaderString, type);
    }
  },

  /**
   * Retrieves a Factory for the given ClassID. Part of nsIModule
   * implementation.
   * @param componentManager  the XPCOM component manager
   * @param cid               the ClassID of the object for which a factory
   *                          has been requested
   * @param iid               the IID of the interface requested
   */
  getClassObject: function(componentManager, cid, iid) {
    if (!iid.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    for (var key in this.objects) {
      if (cid.equals(this.objects[key].CID))
        return this.objects[key].factory;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  /**
   * Create a Factory object that can construct an instance of an object.
   * @param constructor   the constructor used to create the object
   * @private
   */
  _makeFactory: function(constructor) {
    function createInstance(outer, iid) {
      if (outer != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
      return (new constructor()).QueryInterface(iid);
    }
    return { createInstance: createInstance };
  },

  /**
   * Determines whether or not this module can be unloaded.
   * @return returning true indicates that this module can be unloaded.
   */
  canUnload: function(componentManager) {
    return true;
  }
};

/**
 * Entry point for registering the components supplied by this JavaScript
 * module.
 * @param componentManager  the XPCOM component manager
 * @param location          the location of this module on disk
 */
function NSGetModule(componentManager, location) {
  // Metadata about the objects this module can construct
  gModule.objects = {
    search: {
      CID: SEARCH_SUGGEST_CLASSID,
      contractID: SEARCH_SUGGEST_CONTRACTID,
      className: SEARCH_SUGGEST_CLASSNAME,
      factory: gModule._makeFactory(SearchSuggestAutoComplete)
    },
  };
  return gModule;
}
