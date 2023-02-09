Observers
=========

Historically, each successful operation is notified through the *nsINavBookmarksObserver* interface. To listen to such notifications you must register using *nsINavBookmarksService* addObserver and removeObserver methods. Note that bookmark addition or order changes won't notify bookmark-moved for items that have their indexes changed.
Similarly, lastModified changes not done explicitly (like changing another property) won't fire an onItemChanged notification for the lastModified property.

However, right now we are in the process of implementing a Places Observers system to change the *nsINavBookmarksObserver* interface.

Generally - the Observer pattern follows a subscription model. A subscriber (commonly referred to as the observer) subscribes to an event or an action handled by a publisher (commonly referred to as the subject) is notified when the event or action occurs.

Each successful operation is noticed by observer for these events and passed to a subscriber.

`PlacesObservers.webidl`_ a Global Singleton which provides utilities to observe or notify all events.
`PlacesEvent.webidl`_ states all types of possible events and describes their features. In our case, events are:

  - ``“page-visited”`` - ``data: PlacesVisit`` Fired whenever a page is visited
  - ``“bookmark-added”`` - ``data: PlacesBookmarkAddition`` Fired whenever a bookmark (or a bookmark folder/separator) is created.
  - ``“bookmark-removed”`` - ``data: PlacesBookmarkRemoved`` Fired whenever a bookmark (or a bookmark folder/separator) is removed.
  - ``“bookmark-moved”`` - ``data: PlacesBookmarkMoved`` Fired whenever a bookmark (or a bookmark folder/separator) is moved.
  - ``“bookmark-guid-changed”`` - ``data: PlacesBookmarkGuid`` Fired whenever a bookmark guid changes.
  - ``“bookmark-keyword-changed”`` - ``data: PlacesBookmarkKeyword`` Fired whenever a bookmark keyword changes.

  - ``“bookmark-tags-changed”`` - ``data: PlacesBookmarkTags`` Fired whenever tags of bookmark changes.
  - ``“bookmark-time-changed”`` - ``data: PlacesBookmarkTime`` Fired whenever dateAdded or lastModified of a bookmark is explicitly changed through the Bookmarks API. This notification doesn't fire when a bookmark is created, or when a property of a bookmark (e.g. title) is changed, even if lastModified will be updated as a consequence of that change.
  - ``“bookmark-title-changed”`` - ``data: PlacesBookmarkTitle`` Fired whenever a bookmark title changes.
  - ``“bookmark-url-changed”`` - ``data: PlacesBookmarkUrl`` Fired whenever a bookmark url changes.
  - ``“favicon-changed”`` - ``data: PlacesFavicon`` Fired whenever a favicon changes.
  - ``“page-title-changed”`` - ``data: PlacesVisitTitle`` Fired whenever a page title changes.
  - ``“history-cleared”`` - ``data: PlacesHistoryCleared`` Fired whenever history is cleared.
  - ``“page-rank-changed”`` - ``data: PlacesRanking`` Fired whenever pages ranking is changed.
  - ``“page-removed”`` - ``data: PlacesVisitRemoved`` Fired whenever a page or its visits are removed. This may be invoked when a page is removed from the store because it has no more visits, nor bookmarks. It may also be invoked when all or some of the page visits are removed, but the page itself is not removed from the store, because it may be bookmarked.
  - ``“purge-caches”`` - ``data: PlacesPurgeCaches`` Fired whenever changes happened that could not be observed through other notifications, for example a database fixup. When received, observers, especially data views, should drop any caches and reload from scratch.

  .. _PlacesObservers.webidl: https://searchfox.org/mozilla-central/source/dom/chrome-webidl/PlacesObservers.webidl
  .. _PlacesEvent.webidl: https://searchfox.org/mozilla-central/source/dom/chrome-webidl/PlacesEvent.webidl
