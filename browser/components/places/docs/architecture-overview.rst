Architecture Overview
=====================

Because Mozilla used to ship a framework called XULRunner, Places is split into a Toolkit and a Browser part. The toolkit part is usually made up of product-agnostic APIs, those can be used by any kind of project, not just Web Browsers. The browser part is the one containing code specific to the Firefox browser.

Codebase
--------

Most of the codebase is written in:
  * **JavaScript**
  * **C++** (for ex., database initialization code or some of the visit tracking code).

Frontend
--------

The frontend part of the bookmarking experience includes various kind of views:
  * `Trees`_
  * `Menus`_
  * `Toolbars`_

  .. _Trees: https://searchfox.org/mozilla-central/source/browser/components/places/content/places-tree.js
  .. _Menus: https://searchfox.org/mozilla-central/rev/4c184ca81b28f1ccffbfd08f465709b95bcb4aa1/browser/components/places/content/browserPlacesViews.js#1990
  .. _Toolbars: https://searchfox.org/mozilla-central/rev/4c184ca81b28f1ccffbfd08f465709b95bcb4aa1/browser/components/places/content/browserPlacesViews.js#894

All the views share a common `Controller`_ that is responsible to handle operations and commands required by the views. Each view creates a Result object and receives notifications about changes from it.

As an example, removing a bookmark from a view will call into the controller that calls into PlacesTransactions to actually do the removal. The removal will notify a `Places event`_, that is caught by the Result, that will immediately update its internal representation of the bookmarks tree. Then the Result sends a notification to the view that will handle it, updating what the user is seeing. The system works according to the classical `Model-View-Controller`_ pattern.

Fronted dialogs and panels are written using xhtml and shadow DOM. The bookmark dialogs in particular are wrappers around a common template, `editBookmarkPanel.inc.xhtml`_, it could be extended or overloaded like an object (overlay, similar to Web Component).

Most of the logic for the edit bookmark overlay lives in the generic script `editBookmark.js`_.

.. _Controller: https://searchfox.org/mozilla-central/source/browser/components/places/content/controller.js
.. _Places event: https://searchfox.org/mozilla-central/source/dom/chrome-webidl/PlacesEvent.webidl
.. _Model-View-Controller: https://en.wikipedia.org/wiki/Model–view–controller
.. _editBookmarkPanel.inc.xhtml: https://searchfox.org/mozilla-central/source/browser/components/places/content/editBookmarkPanel.inc.xhtml
.. _editBookmark.js: https://searchfox.org/mozilla-central/source/browser/components/places/content/editBookmark.js

Structure of Frontend
^^^^^^^^^^^^^^^^^^^^^

Most part of frontend code is located in : `Browser/Components/Places/Content`_:

  - `BookmarkProperties`_ , BookmarkProperties.xhtml - responsible for editBookmarks & newBookmark Dialog. The panel is initialized based on data given in the js object passed as ``window.arguments[0]``. ``Window.arguments[0]`` is set to the guid of the item, if the dialog is accepted.
  - BookmarksHistoyTooltip.xhtml - code responsible for tooltip
  - `BookmarksSidebar`_, bookmarksSidebar.xhtml - code responsible for sidebar window. Searches through existing bookmarks tree for desired bookmark.
  - `BrowserPlacesViews`_ - controls most views (menu, panels, toolbox). The base view implements everything that's common to the toolbar and menu views.
  - `Controller`_ - controller shared by all places views. Connect UI and actual operations.
  - `EditBookmark`_, editBookmarkPanel.inc.xhtml - controls edit bookmark panel. Observes changes for bookmarks and connects all UI manipulations with backend.
  - `HistorySidebar`_, historySidebar.xhtml - code responsible for history sidebar window. Searches through existing tree for requested History.
  - `Places-menupopup`_ - custom element definition for Places menus
  - `Places-tree`_ - class ``MozPlacesTree`` - builds a custom element definition for the places tree. This is loaded into all XUL windows. Has to be wrapped in a block to prevent leaking to a window scope.
  - Places.css, places.js, places.xhtml - responsible for Library window
  - PlacesCommands.inc.xhtml - commands for multiple windows
  - PlacesContextMenu.inc.xhtml - definition for context menu
  - `TreeView`_ - implementation of the tree view

  .. _Browser/Components/Places/Content: https://searchfox.org/mozilla-central/source/browser/components/places/content
  .. _BookmarkProperties: https://searchfox.org/mozilla-central/source/browser/components/places/content/bookmarkProperties.js
  .. _BookmarksSidebar: https://searchfox.org/mozilla-central/source/browser/components/places/content/bookmarksSidebar.js
  .. _BrowserPlacesViews: https://searchfox.org/mozilla-central/source/browser/components/places/content/browserPlacesViews.js
  .. _EditBookmark: https://searchfox.org/mozilla-central/source/browser/components/places/content/editBookmark.js
  .. _HistorySidebar: https://searchfox.org/mozilla-central/source/browser/components/places/content/historySidebar.js
  .. _Places-menupopup: https://searchfox.org/mozilla-central/source/browser/components/places/content/places-menupopup.js
  .. _Places-tree: https://searchfox.org/mozilla-central/source/browser/components/places/content/places-tree.js
  .. _TreeView: https://searchfox.org/mozilla-central/source/browser/components/places/content/treeView.js


Backend
-------

Because any bookmark operation done by the user in the frontend is undo-able, Firefox usually doesn’t directly invoke the Bookmarks / History API, it instead goes through a wrapper called PlacesTransactions.

The scope of this module is to translate requests for bookmark changes into operation descriptors, and store them in a log. When a bookmark has been modified by the user, PlacesTransactions stores the previous state of it in the log; that state can be restored if the user picks Undo (or CTRL+Z) in the Library window. This prevents data loss in case the user removes bookmarks inadvertently.

Toolkit Places also provides a way to query bookmarks, through Results. This is one of the oldest parts of the codebase that will likely be rewritten in the future. It uses XPCOM (Cross Platform Component Object Model) a Mozilla technology that allows C++ and Javascript to communicate. It’s a declarative system, where interfaces must be defined and documented in XPIDL files. In practice all the available methods and attributes are documented in nsINavHistoryService.idl. Querying bookmarks returns a nsINavHistoryResult object that has a root node. The root node has children representing bookmarks or folders. It works like a tree, containers must be opened and then one can inspect their children. This is the base used by most of the Firefox frontend bookmark views.

Structure of Backend
^^^^^^^^^^^^^^^^^^^^

Most part of backend code is located in : `Toolkit/Components/Places`_:

  - :doc:`Bookmarks` - Asynchronous API for managing bookmarks
  - :doc:`History` - Asynchronous API for managing history
  - `PlacesUtils`_ - This module exports functions for Sync to use when applying remote records
  - :doc:`PlacesTransactions` - This module serves as the transactions manager for Places

  .. _Toolkit/Components/Places: https://searchfox.org/mozilla-central/source/toolkit/components/places
  .. _PlacesUtils: https://searchfox.org/mozilla-central/source/toolkit/components/places/PlacesUtils.jsm

Storage
-------

Places uses `SQLite`_ (C-language library with a stable, cross-platform, and backwards compatible file format) as its data storage backend.
All the data is contained in a places.sqlite file, in the roaming Firefox profile folder. The database is accessed using a wrapper of the SQLite library called `mozStorage`_.
For storing our favicons we use favicons.sqlite which is represented as ATTACH-ed to places.sqlite. That makes it easier to use our two separate sqlites as one single database.

Synchronization
---------------

Places works in strict contact with `Firefox Sync`_, to synchronize bookmarks and history across devices, thus you can meet Sync specific code in various parts of the Places codebase. Some of the code may refer to Weave, the old project name for Sync.

.. _SQLite: https://www.sqlite.org/index.html
.. _mozStorage: https://searchfox.org/mozilla-central/source/storage
.. _Firefox Sync: https://www.mozilla.org/en-US/firefox/sync/
