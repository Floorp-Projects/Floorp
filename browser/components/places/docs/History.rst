History.sys.mjs
===============

Asynchronous API for managing history.

Structure
---------

The API makes use of `PageInfo` and `VisitInfo` objects, defined as follows.

A `PageInfo` object is any object that contains A *subset* of the following properties:

  * ``guid (string)`` - the globally unique id of the page
  * ``url (URL, href or nsIURI)`` -  the full URI of the page. Note that `PageInfo` values passed as argument may hold ``nsIURI`` or ``string`` values for property ``url``, but `PageInfo` objects returned by this module always hold ``URL`` values
  * ``title (string)`` - the title associated with the page, if any
  * ``description (string)`` - the description of the page, if any
  * ``previewImageURL (URL, nsIURI or string)`` - the preview image URL of the page, if any
  * ``frecency (number)`` -  the frecency of the page, if any. Note that this property may not be used to change the actual frecency score of a page, only to retrieve it. In other words, any "frecency" field passed as argument to a function of this API will be ignored
  * ``visits (Array<VisitInfo>)`` - all the visits for this page, if any
  * ``annotations (Map)`` - a map containing key/value pairs of the annotations for this page, if any

A `VisitInfo` object is any object that contains A SUBSET of the following properties:

  * ``date (Date)`` - the time the visit occurred
  * ``transition (number)`` - how the user reached the page
  * ``referrer (URL, nsIURI or string)`` - the referring URI of this visit. Note that `VisitInfo` passed as argument may hold ``nsIURI`` or ``string`` values for property ``referrer``, but `VisitInfo` objects returned by this module always hold ``URL`` values

Main functions
--------------

  * Fetching. Fetch the available information for one page. You would have to specify as a string a unique identifier (Guid) or url (url, nsIURI or href). Fetching step returns a promise which is successfully resolved when fetch is completed and page is found. Throws an error when Guid or url does not have an expected type or, if it is a string, url / Guid is not valid.

  * Removing pages / visits from database. You would have to specify the desired page / object visit to be removed. Returns a promise which is resolved with true boolean. Throws an error when 'pages' has an unexpected type or if there are no data for provided string / filter.

  * Determining if a page has been visited. Connects with database and inquries if specified page has been visited or not. Returns a promise which is resolved with a true boolean value if page has been visited, falsy value if not. Throws an error if provided data is not a valid Guid or uri.

  * Clearing all history. Returns a promise which is resolved when operation is completed.

  * Updating an information for a page. Currently, it supports updating the description, preview image URL and annotations for a page, any other fields will be ignored (this function will ignore the update if the target page has not yet been stored in the database. ``History.fetch`` could be used to check whether the page and its meta information exist or not). If a property 'description' is provided, the description of the page is updated. An empty string or null will clear the existing value in database, and description length should not be longer than ``DB_DESCRIPTION_LENGTH_MAX``. If a property 'siteName' is provided the site name of the page is updated, and 'previewImageURL' updates the preview image URL of the page. Applies same rules as ones for description regarding empty string clearing existing data and max length as ``DB_SITENAME_LENGTH_MAX`` for site name, ``DB_URL_LENGTH_MAX`` for preview image. Property 'annotations' uppdates the annotation. It should be a map, containign key/values pairs, if the value is falsy, the annotation will be removed. The keys must be Strings, the values should be Boolaen, Number or Strings. Updating information returns a promise which is rejected if operation was unsuccessful or resolved once the update is complete. Throws an error when 'pageInfo' has an unexpected type, invalid url / guid or has neither 'description' nor 'previewImageURL'.

Each successful operation is notified through the PlacesObservers :doc:`notifyObservers` interface.

Full file with actual javadoc and description of each method - `History.sys.mjs`_
