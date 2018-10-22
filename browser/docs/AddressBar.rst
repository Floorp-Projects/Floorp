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

Each search is represented by a unique object, the *QueryContext*. This object,
created by the *View*, describes the search and is passed through all of the
components, along the way it gets augmented with additional information.
The *QueryContext* is passed to the *Controller*, and finally to the *Model*.
The model appends matches to a property of *QueryContext* in chunks, it sorts
them through a *Muxer* and then notifies the *Controller*.

See the specific components below, for additional details about each one's tasks
and responsibilities.


The QueryContext
================

The *QueryContext* object describes a single instance of a search.
It is augmented as it progresses through the system, with various information:

.. highlight:: JavaScript
.. code::

  QueryContext {
    searchString; // {string} The user typed string.
    lastKey; // {string} The last key pressed by the user. This can affect the
             // behavior, for example by not autofilling again when the user
             // hit backspace.
    maxResults; // {integer} The maximum number of results requested. The Model
                // may actually return more results than expected, so that the
                // View and the Controller can do additional filtering.
    isPrivate; // {boolean} Whether the search started in a private context.
    userContextId; // {integer} The user context ID (containers feature).

    // Properties added by the Model.
    tokens; // {array} tokens extracted from the searchString, each token is an
            // object in the form {type, value}.
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

The *Controller* starts and stops queries through the *ProvidersManager*. It's
possible to wait for the promise returned by *startQuery* to know when no more
matches will be returned, it is not mandatory though. Queries can be canceled.

.. note::

  Canceling a query will issue an interrupt() on the database connection,
  terminating any running and future SQL query, unless a query is running inside
  a *runInCriticalSection* task.

The *searchString* gets tokenized by the `UrlbarTokenizer <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarTokenizer.jsm>`_
component into tokens, some of these tokens have a special meaning and can be
used by the user to restrict the search to specific type of matches (See the
*UrlbarTokenizer::TYPE* enum).

.. caution::

  The tokenizer uses heuristics to determine each token's type, as such the
  consumer may want to check the value before applying filters.

.. highlight:: JavaScript
.. code::

  UrlbarProvidersManager {
    registerProvider(providerObj);
    unregisterProvider(providerObj);
    async startQuery(queryContext);
    cancelQuery(queryContext);
    // Can be used by providers to run uninterruptible queries.
    runInCriticalSection(taskFn);
  }

UrlbarProvider
--------------

A provider is specialized into searching and returning matches from different
information sources. Internal providers are usually implemented in separate
*jsm* modules with a *UrlbarProvider* name prefix. External providers can be
registered as *Objects* through the *UrlbarProvidersManager*.
Each provider is independent and must satisfy a base API, while internal
implementation details may vary deeply among different providers.

.. important::

  Providers are singleton, and must track concurrent searches internally, for
  example mapping them by QueryContext.

.. note::

  Internal providers can access the Places database through the
  *PlacesUtils.promiseLargeCacheDBConnection* utility.

.. highlight:: JavaScript
.. code::

  UrlbarProvider {
    name; // {string} A simple name to track the provider.
    type; // {integer} One of UrlbarUtils.PROVIDER_TYPE.
    // The returned promise should be resolved when the provider is done
    // searching AND returning matches.
    // Each new UrlbarMatch should be passed to the AddCallback function.
    async startQuery(QueryContext, AddCallback);
    // Any cleaning/resetting task should happen here.
    cancelQuery(QueryContext);
  }

UrlbarMuxer
-----------

The *Muxer* is responsible for sorting matches based on their importance and
additional rules that depend on the QueryContext.

.. caution

  The Muxer is a replaceable component, as such what is described here is a
  reference for the default View, but may not be valid for other implementations.

*Content to be written*


The Controller
==============

`UrlbarController <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarController.jsm>`_
is the component responsible for reacting to user's input, by communicating
proper course of action to the Model (e.g. starting/stopping a query) and the
View (e.g. showing/hiding a panel). It is also responsible for reporting Telemetry.

.. note::

  Each *View* has a different *Controller* instance.

.. highlight:: JavaScript
.. code:

  UrlbarController {
    async startQuery(QueryContext);
    cancelQuery(queryContext);
    // Invoked by the ProvidersManager when matches are available.
    receiveResults(queryContext);
    // Used by the View to listen for matches.
    addQueryListener(listener);
    removeQueryListener(listener);
    // Used to indicate the View context changed, as such any cached information
    // should be reset.
    tabContextChanged();
  }


The View
=========

The View is the component responsible for presenting search results to the
user and handling their input.

.. caution

  The View is a replaceable component, as such what is described here is a
  reference for the default View, but may not be valid for other implementations.

`UrlbarInput.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarInput.jsm>`_
----------------

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
    // Converts an internal URI (e.g. a wyciwyg URI) into one which we can
    // expose to the user.
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
---------------

Represents the base *View* implementation, communicates with the *Controller*.

.. highlight:: JavaScript
.. code::

  UrlbarView {
    // Manage View visibility.
    open();
    close();
    // Invoked when the query starts.
    onQueryStarted(queryContext);
    // Invoked when new matches are available.
    onQueryResults(queryContext);
  }

UrlbarMatch
===========

An `UrlbarMatch <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarMatch.jsm>`_
instance represents a single match (search result) with a match type, that
identifies specific kind of results.
Each kind has its own properties, that the *View* may support, and a few common
properties, supported by all of the matches.

.. note::

  Match types are also enumerated by *UrlbarUtils.MATCH_TYPE*.

.. highlight:: JavaScript
.. code::

  UrlbarMatch {
    constructor(matchType, payload);

    // Common properties:
    url: {string} The url pointed by this match.
    title: {string} A title that may be used as a label for this match.
  }


Shared Modules
==============

Various modules provide shared utilities to the other components:

`UrlbarPrefs.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/UrlbarPrefs.jsm>`_
----------------

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
----------------

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
