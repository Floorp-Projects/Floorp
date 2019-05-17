.. _addressbar:

===========
Address Bar
===========

The *Address Bar* component drives the browser's url bar, a specialized search
access point (SAP) including different sources of information:
  * Places: the History, Bookmarks, Favicons and Tags component
  * Search Engines and Suggestions
  * WebExtensions
  * Open Tabs
  * Keywords and aliases

The *Address Bar* component lives in the
`browser/components/urlbar/ <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/>`_ folder.

.. note::

  The current *Address Bar* is driven by the legacy
  `toolkit autocomplete <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/autocomplete>`_
  binding, along with the *Places UnifiedComplete* component. This documentation
  describes a new rewrite of the feature, also known as the **Quantum Bar**.

.. caution::

  This code is under heavy development and may change abruptly.


Global Architecture Overview
============================

The *Address Bar* is implemented as a *Model-View-Controller* (MVC) system. One of
the scopes of this architecture is to allow easy replacement of its components,
for easier experimentation.

Each search is represented by a unique object, the *UrlbarQueryContext*. This
object, created by the *View*, describes the search and is passed through all of
the components, along the way it gets augmented with additional information.
The *UrlbarQueryContext* is passed to the *Controller*, and finally to the
*Model*.  The model appends results to a property of *UrlbarQueryContext* in
chunks, it sorts them through a *Muxer* and then notifies the *Controller*.

See the specific components below, for additional details about each one's tasks
and responsibilities.


The UrlbarQueryContext
======================

The *UrlbarQueryContext* object describes a single instance of a search.
It is augmented as it progresses through the system, with various information:

.. highlight:: JavaScript
.. code::

  UrlbarQueryContext {
    allowAutofill; // {boolean} If true, providers are allowed to return
                   // autofill results.  Even if true, it's up to providers
                   // whether to include autofill results, but when false, no
                   // provider should include them.
    isPrivate; // {boolean} Whether the search started in a private context.
    maxResults; // {integer} The maximum number of results requested. It is
                // possible to request more results than the shown ones, and
                // do additional filtering at the View level.
    searchString; // {string} The user typed string.
    userContextId; // {integer} The user context ID (containers feature).

    // Optional properties.
    muxer; // {string} Name of a registered muxer. Muxers can be registered
           // through the UrlbarProvidersManager.
    providers; // {array} List of registered provider names. Providers can be
               // registered through the UrlbarProvidersManager.
    sources; // {array} If provided is the list of sources, as defined by
             // RESULT_SOURCE.*, that can be returned by the model.

    // Properties added by the Model.
    preselected; // {boolean} whether the first result should be preselected.
    results; // {array} list of UrlbarResult objects.
    tokens; // {array} tokens extracted from the searchString, each token is an
            // object in the form {type, value, lowerCaseValue}.
    acceptableSources; // {array} list of UrlbarUtils.RESULT_SOURCE that the
                       // model will accept for this context.
  }


The Model
=========

The *Model* is the component responsible for retrieving search results based on
the user's input, and sorting them accordingly to their importance.
At the core is the `UrlbarProvidersManager <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarProvidersManager.jsm>`_,
a component tracking all the available search providers, and managing searches
across them.

The *UrlbarProvidersManager* is a singleton, it registers internal providers on
startup and can register/unregister providers on the fly.
It can manage multiple concurrent queries, and tracks them internally as
separate *Query* objects.

The *Controller* starts and stops queries through the *UrlbarProvidersManager*.
It's possible to wait for the promise returned by *startQuery* to know when no
more results will be returned, it is not mandatory though.
Queries can be canceled.

.. note::

  Canceling a query will issue an interrupt() on the database connection,
  terminating any running and future SQL query, unless a query is running inside
  a *runInCriticalSection* task.

The *searchString* gets tokenized by the `UrlbarTokenizer <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarTokenizer.jsm>`_
component into tokens, some of these tokens have a special meaning and can be
used by the user to restrict the search to specific result type (See the
*UrlbarTokenizer::TYPE* enum).

.. caution::

  The tokenizer uses heuristics to determine each token's type, as such the
  consumer may want to check the value before applying filters.

.. highlight:: JavaScript
.. code::

  UrlbarProvidersManager {
    registerProvider(providerObj);
    unregisterProvider(providerObj);
    registerMuxer(muxerObj);
    unregisterMuxer(muxerObjOrName);
    async startQuery(queryContext);
    cancelQuery(queryContext);
    // Can be used by providers to run uninterruptible queries.
    runInCriticalSection(taskFn);
  }

UrlbarProvider
--------------

A provider is specialized into searching and returning results from different
information sources. Internal providers are usually implemented in separate
*jsm* modules with a *UrlbarProvider* name prefix. External providers can be
registered as *Objects* through the *UrlbarProvidersManager*.
Each provider is independent and must satisfy a base API, while internal
implementation details may vary deeply among different providers.

.. important::

  Providers are singleton, and must track concurrent searches internally, for
  example mapping them by UrlbarQueryContext.

.. note::

  Internal providers can access the Places database through the
  *PlacesUtils.promiseLargeCacheDBConnection* utility.

.. highlight:: JavaScript
.. code::

  class UrlbarProvider {
    /**
     * Unique name for the provider, used by the context to filter on providers.
     * Not using a unique name will cause the newest registration to win.
     * @abstract
     */
    get name() {
      return "UrlbarProviderBase";
    }
    /**
     * The type of the provider, must be one of UrlbarUtils.PROVIDER_TYPE.
     * @abstract
     */
    get type() {
      throw new Error("Trying to access the base class, must be overridden");
    }
    /**
     * Whether this provider should be invoked for the given context.
     * If this method returns false, the providers manager won't start a query
     * with this provider, to save on resources.
     * @param {UrlbarQueryContext} queryContext The query context object
     * @returns {boolean} Whether this provider should be invoked for the search.
     * @abstract
     */
    isActive(queryContext) {
      throw new Error("Trying to access the base class, must be overridden");
    }
    /**
     * Whether this provider wants to restrict results to just itself.
     * Other providers won't be invoked, unless this provider doesn't
     * support the current query.
     * @param {UrlbarQueryContext} queryContext The query context object
     * @returns {boolean} Whether this provider wants to restrict results.
     * @abstract
     */
    isRestricting(queryContext) {
      throw new Error("Trying to access the base class, must be overridden");
    }
    /**
     * Starts querying.
     * @param {UrlbarQueryContext} queryContext The query context object
     * @param {function} addCallback Callback invoked by the provider to add a new
     *        result. A UrlbarResult should be passed to it.
     * @note Extended classes should return a Promise resolved when the provider
     *       is done searching AND returning results.
     * @abstract
     */
    startQuery(queryContext, addCallback) {
      throw new Error("Trying to access the base class, must be overridden");
    }
    /**
     * Cancels a running query,
     * @param {UrlbarQueryContext} queryContext The query context object to cancel
     *        query for.
     * @abstract
     */
    cancelQuery(queryContext) {
      throw new Error("Trying to access the base class, must be overridden");
    }
  }

UrlbarMuxer
-----------

The *Muxer* is responsible for sorting results based on their importance and
additional rules that depend on the UrlbarQueryContext. The muxer to use is
indicated by the UrlbarQueryContext.muxer property.

.. caution::

  The Muxer is a replaceable component, as such what is described here is a
  reference for the default View, but may not be valid for other implementations.

.. highlight:: JavaScript
.. code::

  class UrlbarMuxer {
    /**
     * Unique name for the muxer, used by the context to sort results.
     * Not using a unique name will cause the newest registration to win.
     * @abstract
     */
    get name() {
      return "UrlbarMuxerBase";
    }
    /**
     * Sorts UrlbarQueryContext results in-place.
     * @param {UrlbarQueryContext} queryContext the context to sort results for.
     * @abstract
     */
    sort(queryContext) {
      throw new Error("Trying to access the base class, must be overridden");
    }
  }


The Controller
==============

`UrlbarController <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarController.jsm>`_
is the component responsible for reacting to user's input, by communicating
proper course of action to the Model (e.g. starting/stopping a query) and the
View (e.g. showing/hiding a panel). It is also responsible for reporting Telemetry.

.. note::

  Each *View* has a different *Controller* instance.

.. highlight:: JavaScript
.. code::

  UrlbarController {
    async startQuery(queryContext);
    cancelQuery(queryContext);
    // Invoked by the ProvidersManager when results are available.
    receiveResults(queryContext);
    // Used by the View to listen for results.
    addQueryListener(listener);
    removeQueryListener(listener);
    // Used to indicate the View context changed, so that cached information
    // about the latest search is no more relevant and can be dropped.
    viewContextChanged();
  }


The View
=========

The View is the component responsible for presenting search results to the
user and handling their input.

.. caution

  The View is a replaceable component, as such what is described here is a
  reference for the default View, but may not be valid for other implementations.

`UrlbarInput.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarInput.jsm>`_
-------------------------------------------------------------------------------------------------------------

Implements an input box *View*, owns an *UrlbarView*.

.. highlight:: JavaScript
.. code::

  UrlbarInput {
    constructor(options = { textbox, panel, controller });
    // Used to trim urls when necessary (e.g. removing "http://")
    trimValue();
    // Uses UrlbarValueFormatter to highlight the base host, search aliases
    // and to keep the host visible on overflow.
    formatValue(val);
    // Manage view visibility.
    closePopup();
    openResults();
    // Converts an internal URI (e.g. a URI with a username or password) into
    // one which we can expose to the user.
    makeURIReadable(uri);
    // Handles an event which would cause a url or text to be opened.
    handleCommand();
    // Called by the view when a result is selected.
    resultsSelected();
    // The underlying textbox
    textbox;
    // The results panel.
    panel;
    // The containing window.
    window;
    // The containing document.
    document;
    // An UrlbarController instance.
    controller;
    // An UrlbarView instance.
    view;
    // Whether the current value was typed by the user.
    valueIsTyped;
    // Whether the input box has been focused by a user action.
    userInitiatedFocus;
    // Whether the context is in Private Browsing mode.
    isPrivate;
    // Whether the input box is focused.
    focused;
    // The go button element.
    goButton;
    // The current value, can also be set.
    value;
  }

`UrlbarView.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarView.jsm>`_
-----------------------------------------------------------------------------------------------------------

Represents the base *View* implementation, communicates with the *Controller*.

.. highlight:: JavaScript
.. code::

  UrlbarView {
    // Manage View visibility.
    open();
    close();
    // Invoked when the query starts.
    onQueryStarted(queryContext);
    // Invoked when new results are available.
    onQueryResults(queryContext);
    // Invoked when the query has been canceled.
    onQueryCancelled(queryContext);
    // Invoked when the query is done. This is invoked in any case, even if the
    // query was canceled earlier.
    onQueryFinished(queryContext);
    // Invoked when the view context changed, so that cached information about
    // the latest search is no more relevant and can be dropped.
    onViewContextChanged();
  }


UrlbarResult
============

An `UrlbarResult <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarResult.jsm>`_
instance represents a single search result with a result type, that
identifies specific kind of results.
Each kind has its own properties, that the *View* may support, and a few common
properties, supported by all of the results.

.. note::

  Result types are also enumerated by *UrlbarUtils.RESULT_TYPE*.

.. highlight:: JavaScript
.. code::

  UrlbarResult {
    constructor(resultType, payload);

    type: {integer} One of UrlbarUtils.RESULT_TYPE.
    source: {integer} One of UrlbarUtils.RESULT_SOURCE.
    title: {string} A title that may be used as a label for this result.
    icon: {string} Url of an icon for this result.
    payload: {object} Object containing properties for the specific RESULT_TYPE.
    autofill: {object} An object describing the text that should be
              autofilled in the input when the result is selected, if any.
    autofill.value: {string} The autofill value.
    autofill.selectionStart: {integer} The first index in the autofill
                             selection.
    autofill.selectionEnd: {integer} The last index in the autofill selection.
  }

The following RESULT_TYPEs are supported:

.. highlight:: JavaScript
.. code::

    // Payload: { icon, url, userContextId }
    TAB_SWITCH: 1,
    // Payload: { icon, suggestion, keyword, query, isKeywordOffer }
    SEARCH: 2,
    // Payload: { icon, url, title, tags }
    URL: 3,
    // Payload: { icon, url, keyword, postData }
    KEYWORD: 4,
    // Payload: { icon, keyword, title, content }
    OMNIBOX: 5,
    // Payload: { icon, url, device, title }
    REMOTE_TAB: 6,


Shared Modules
==============

Various modules provide shared utilities to the other components:

`UrlbarPrefs.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarPrefs.jsm>`_
-------------------------------------------------------------------------------------------------------------

Implements a Map-like storage or urlbar related preferences. The values are kept
up-to-date.

.. highlight:: JavaScript
.. code::

  // Always use browser.urlbar. relative branch, except for the preferences in
  // PREF_OTHER_DEFAULTS.
  UrlbarPrefs.get("delay"); // Gets value of browser.urlbar.delay.

.. note::

  Newly added preferences should always be properly documented in UrlbarPrefs.

`UrlbarUtils.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarUtils.jsm>`_
-------------------------------------------------------------------------------------------------------------

Includes shared utils and constants shared across all the components.


Telemetry Probes
================

*Content to be written*


Debugging & Logging
===================

*Content to be written*


Getting in Touch
================

For any questions regarding the Address Bar, the team is available through
the #fx-search channel on irc.mozilla.org and the fx-search@mozilla.com mailing
list.

Issues can be `filed in Bugzilla <https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox&component=Address%20Bar>`_
under the Firefox / Address Bar component.
