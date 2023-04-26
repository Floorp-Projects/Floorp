Search Lifecycle
================

When a character is typed into the address bar, or the address bar is focused,
we initiate a search. What follows is a simplified version of the
lifetime of a search, describing the pipeline that returns results for a typed
string. Some parts of the query lifetime are intentionally omitted from this
document for clarity.

The search described in this document is internal to the address bar. It is not
the search sent to the default search engine when you press Enter. Parts of this
process often occur multiple times per keystroke, as described below.

It is recommended that you first read the :doc:`nontechnical-overview` to become
familiar with the terminology in this document. This document is current as
of April 2023.

#.
   The user types a query (e.g. "coffee near me") into the *UrlbarInput*
   `<input> DOM element <https://searchfox.org/mozilla-central/rev/1f4f99a8f331cce8467a50742178b6d46914ab89/browser/base/content/navigator-toolbox.inc.xhtml#330-336>`_.
   That DOM element `tells <https://searchfox.org/mozilla-central/rev/1f4f99a8f331cce8467a50742178b6d46914ab89/browser/components/urlbar/UrlbarInput.sys.mjs#3312>`_
   *UrlbarInput* that text is being input.

#.
   *UrlbarInput* `starts a search <https://searchfox.org/mozilla-central/rev/1f4f99a8f331cce8467a50742178b6d46914ab89/browser/components/urlbar/UrlbarInput.sys.mjs#3395>`_.
   It `creates <https://searchfox.org/mozilla-central/rev/1f4f99a8f331cce8467a50742178b6d46914ab89/browser/components/urlbar/UrlbarInput.sys.mjs#1549>`_
   a `UrlbarQueryContext <https://firefox-source-docs.mozilla.org/browser/urlbar/overview.html#the-urlbarquerycontext>`_
   and `passes it to UrlbarController <https://searchfox.org/mozilla-central/rev/1f4f99a8f331cce8467a50742178b6d46914ab89/browser/components/urlbar/UrlbarInput.sys.mjs#1548>`_.
   The query context is an object that will exist for the lifetime of the query
   and it's how we keep track of what results to show. It contains information
   like what kind of results are allowed, the search string ("coffee near me",
   in this case), and other information about the state of the Urlbar. A new
   *UrlbarQueryContext* is created every time the text in the input changes.

#.
   *UrlbarController* `tells UrlbarProvidersManager <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarController.sys.mjs#140>`_
   that the providers should fetch results.

#.
   *UrlbarProvidersManager* tells `each <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProvidersManager.sys.mjs#408>`_
   provider to decide if it wants to provide results for this query by calling
   their `isActive <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProvidersManager.sys.mjs#422>`_
   methods. The provider can decide whether or not it will be active for this
   query. Some providers are rarely active: for example,
   *UrlbarProviderTopSites* `isn't active if the user has typed a search string <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProviderTopSites.sys.mjs#97>`_.

#.
   *UrlbarProvidersManager* then tells the *active* providers to fetch results by
   `calling their startQuery method <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProvidersManager.sys.mjs#462>`_.

#.
   The providers fetch results for the query asynchronously. Each provider
   fetches results in a different way. As one example, if the default search
   engine is Google, *UrlbarProviderSearchSuggestions* would send the string
   "coffee near me" to Google. Google would return a list of suggestions and
   *UrlbarProviderSearchSuggestions* would create a *UrlbarResult* for each one.

#.
   The providers send their results back to *UrlbarProvidersManager*. They do
   this one result at a time by `calling the addCallback callback <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProviderSearchSuggestions.sys.mjs#292>`_
   passed into startQuery. *UrlbarProvidersManager* takes all the results from all the
   providers and `puts them into the list of unsorted results <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProvidersManager.sys.mjs#607>`_.

   Due to the asynchronous and parallel nature of providers, this and the
   following steps may occur multiple times per search. Some providers may take
   longer than others to return their results. We don't want to wait for slow
   providers before showing results. To handle slow providers,
   *UrlbarProvidersManager* gathers results from providers in "chunks". A timer
   fires on an internal. Every time the timer fires, we take whatever results we
   have from the active providers (the "chunk" of results) and perform the
   following steps.

#.
   *UrlbarProvidersManager* `asks <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProvidersManager.sys.mjs#648>`_
   *UrlbarMuxer* to sort the unsorted results.

#.
   *UrlbarMuxer* chooses the results that will be shown to the user. It groups
   and sorts the results to determine the order in which the results will be
   shown. This process usually involves discarding irrelevant and duplicate
   results. We also cap results at a limit, defined in the
   ``browser.urlbar.maxRichResults`` preference.

#.
   Once the results are sorted, *UrlbarProvidersManager*
   `tells UrlbarController <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarProvidersManager.sys.mjs#675>`_
   that results are ready to be shown.

#.
   *UrlbarController* `sends out a notification <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarController.sys.mjs#213>`_
   that results are ready to be shown. *UrlbarView* was `listening <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarView.sys.mjs#662>`_
   for that notification. Once the view gets the notification, it `calls #updateResults <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarView.sys.mjs#670>`_
   to create `DOM nodes <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarView.sys.mjs#1185>`_
   for each *UrlbarResult* and `inserts them <https://searchfox.org/mozilla-central/rev/0ffaecaa075887ab07bf4c607c61ea2faa81b172/browser/components/urlbar/UrlbarView.sys.mjs#1156>`_
   into the view's DOM element.

   As described above, we may reach this step multiple times per search. That
   means we may be updating the view multiple times per keystroke. A view that
   visibly changes many times after a single keystroke is perceived as
   "flickering" by the user. As a result, we try to limit the number of times
   the view needs to update.


   .. figure:: assets/lifetime/lifetime.png
      :alt: A chart with boxes representing the various components of the
            address bar. An arrow moves between the boxes to illustrate a query
            moving through the components.
      :scale: 80%
      :align: center
