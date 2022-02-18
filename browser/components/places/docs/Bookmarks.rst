Bookmarks.jsm
=============

Asynchronous API for managing bookmarks.
Bookmarks are organized in a tree structure, and include URLs, folders and separators. Multiple bookmarks for the same URL are allowed.
To be able to use “Redo” and “Undo” the Firefox UI views are not using API directly but use :doc:`PlacesTransactions`.

Structure
---------

Each bookmark-item is represented by an object having the following properties:

  * ``guid (string)`` - the globally unique identifier of the item
  * ``parentGuid (string)`` - the globally unique identifier of the folder containing the item. This will be an empty string for the Places root folder. The root folder is never accessible to the user, it just contains other roots, like toolbar, menu, unfiled, mobile
  * ``index (number)`` - the zero-based position of the item in the parent folder
  * ``dateAdded (Date)`` - the time at which the item was added
  * ``lastModified (Date)`` - the time at which the item was last modified
  * ``type (number)`` - the item's type, either ``TYPE_BOOKMARK``, ``TYPE_FOLDER`` or ``TYPE_SEPARATOR``

The following properties are only valid for URLs or folders:

  * ``title (string)`` - the item's title, if any.  Empty titles and null titles are considered the same. Titles longer than ``DB_TITLE_LENGTH_MAX`` will be truncated

The following properties are only valid for URLs:

  * ``url (URL, href or nsIURI)`` - the item's URL.  Note that while input objects can contain either an URL object, an href string, or an nsIURI, output objects will always contain an URL object. An URL cannot be longer than ``DB_URL_LENGTH_MAX``, methods will throw if a longer value is provided.

Main functions
--------------

  * Every bookmark has a globally unique identifier represented by ``string`` also known as ``Guid``. ``Guid`` helps us to locate our bookmark in database, change it, move, copy or delete.


  * Creating new bookmark. When we inserting a bookmark-item into the bookmark tree, we need to set up a parentGuid property, which is a section where bookmark will be located, for example - Toolbar (``"toolbar_____"``). Location can be any other folder guid, or special root folder guids like:

    - rootGuid: ``"root________"``
    - menuGuid: ``"menu________"``
    - toolbarGuid: ``"toolbar_____"``
    - unfiledGuid: ``"unfiled_____"``
    - mobileGuid: ``"mobile______"``

  * As well, you would have to specify bookmark type (for example, ``TYPE_BOOKMARK`` for a single bookmark, or ``TYPE_FOLDER`` for a folder).

  * Remove bookmark. For removing one or more bookmark items, you would have to specify ``guidOrInfo``. This may be a bookmark guid, an object representing an item, or an array of objects representing the items. You can specify options for removal - throwing an exception when attempting to remove a folder that is not empty and the change source forwarded to all bookmark observers. (Default source is pointing to nsINavBookmarksService::SOURCE_DEFAULT). Removing a bookmark returns a promise which could be: resolved by completion or rejected, in case there are no matching bookmarks to be removed.

  * Fetching information about a bookmark-item. Any successful call to this method resolves to a single bookmark-item (or null), even when multiple bookmarks may exist (e.g. fetching by url). If you wish to retrieve all of the bookmarks for a given match, you would have to use the callback. Input for fetching a bookamrk can be either a guid or an object with one of filtering properties (for example, ``url`` - retrieves the most recent bookmark having the given URL). Fetching returns the promise which is resolved on completion, rejects if any errors happens through fetch or throws an error if any of the specified arguments are invalid.

Each successful operation is notified through the PlacesObservers :doc:`notifyObservers` interface.

Full file with actual javadoc and description of each method - `Bookmarks.jsm`_
