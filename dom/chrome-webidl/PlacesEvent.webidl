enum PlacesEventType {
  "none",

  /**
   * data: PlacesVisit. Fired whenever a page is visited.
   */
  "page-visited",
  /**
   * data: PlacesBookmarkAddition. Fired whenever a bookmark
   * (or a bookmark folder/separator) is created.
   */
  "bookmark-added",
  /**
   * data: PlacesBookmarkRemoved. Fired whenever a bookmark
   * (or a bookmark folder/separator) is removed.
   */
  "bookmark-removed",
  /**
   * data: PlacesBookmarkMoved. Fired whenever a bookmark
   * (or a bookmark folder/separator) is moved.
   */
  "bookmark-moved",
  /**
   * data: PlacesFavicon. Fired whenever a favicon changes.
   */
  "favicon-changed",
  /**
   * data: PlacesVisitTitle. Fired whenever a page title changes.
   */
  "page-title-changed",
  /**
   * data: PlacesHistoryCleared. Fired whenever history is cleared.
   */
  "history-cleared",
  /**
   * data: PlacesRanking. Fired whenever pages ranking is changed.
   */
  "pages-rank-changed",
  /**
   * data: PlacesVisitRemoved. Fired whenever a page or its visits are removed.
   * This may be invoked when a page is removed from the store because it has no more
   * visits, nor bookmarks. It may also be invoked when all or some of the page visits are
   * removed, but the page itself is not removed from the store, because it may be
   * bookmarked.
   */
  "page-removed",
  /**
   * data: PlacesPurgeCaches. Fired whenever changes happened that could not be observed
   * through other notifications, for example a database fixup. When received, observers,
   * especially data views, should drop any caches and reload from scratch.
   */
  "purge-caches",
};

[ChromeOnly, Exposed=Window]
interface PlacesEvent {
  readonly attribute PlacesEventType type;
};

[ChromeOnly, Exposed=Window]
interface PlacesVisit : PlacesEvent {
  /**
   * URL of the visit.
   */
  readonly attribute DOMString url;

  /**
   * Id of the visit.
   */
  readonly attribute unsigned long long visitId;

  /**
   * Time of the visit, in milliseconds since epoch.
   */
  readonly attribute unsigned long long visitTime;

  /**
   * The id of the visit the user came from, defaults to 0 for no referrer.
   */
  readonly attribute unsigned long long referringVisitId;

  /**
   * One of nsINavHistory.TRANSITION_*
   */
  readonly attribute unsigned long transitionType;

  /**
   * The unique id associated with the page.
   */
  readonly attribute ByteString pageGuid;

  /**
   * Whether the visited page is marked as hidden.
   */
  readonly attribute boolean hidden;

  /**
   * Number of visits (including this one) for this URL.
   */
  readonly attribute unsigned long visitCount;

  /**
   * Whether the URL has been typed or not.
   * TODO (Bug 1271801): This will become a count, rather than a boolean.
   * For future compatibility, always compare it with "> 0".
   */
  readonly attribute unsigned long typedCount;

  /**
   * The last known title of the page. Might not be from the current visit,
   * and might be null if it is not known.
   */
  readonly attribute DOMString? lastKnownTitle;
};

/**
 * Base class for properties that are common to all bookmark events.
 */
[ChromeOnly, Exposed=Window]
interface PlacesBookmark : PlacesEvent {
  /**
   * The id of the item.
   */
  readonly attribute long long id;

  /**
   * The id of the folder to which the item belongs.
   */
  readonly attribute long long parentId;

  /**
   * The type of the added item (see TYPE_* constants in nsINavBooksService.idl).
   */
  readonly attribute unsigned short itemType;

  /**
   * The URI of the added item if it was TYPE_BOOKMARK, "" otherwise.
   */
  readonly attribute DOMString url;

  /**
   * The unique ID associated with the item.
   */
  readonly attribute ByteString guid;

  /**
   * The unique ID associated with the item's parent.
   */
  readonly attribute ByteString parentGuid;

  /**
   * A change source constant from nsINavBookmarksService::SOURCE_*,
   * passed to the method that notifies the observer.
   */
  readonly attribute unsigned short source;

  /**
   * True if the item is a tag or a tag folder.
   * NOTE: this will go away with bug 424160.
   */
  readonly attribute boolean isTagging;
};

dictionary PlacesBookmarkAdditionInit {
  required long long id;
  required long long parentId;
  required unsigned short itemType;
  required DOMString url;
  required ByteString guid;
  required ByteString parentGuid;
  required unsigned short source;
  required long index;
  required DOMString title;
  required unsigned long long dateAdded;
  required boolean isTagging;
};

[ChromeOnly, Exposed=Window]
interface PlacesBookmarkAddition : PlacesBookmark {
  constructor(PlacesBookmarkAdditionInit initDict);

  /**
   * The item's index in the folder.
   */
  readonly attribute long index;

  /**
   * The title of the added item.
   */
  readonly attribute DOMString title;

  /**
   * The time that the item was added, in milliseconds from the epoch.
   */
  readonly attribute unsigned long long dateAdded;
};

dictionary PlacesBookmarkRemovedInit {
  required long long id;
  required long long parentId;
  required unsigned short itemType;
  required DOMString url;
  required ByteString guid;
  required ByteString parentGuid;
  required unsigned short source;
  required long index;
  required boolean isTagging;
  boolean isDescendantRemoval = false;
};

[ChromeOnly, Exposed=Window]
interface PlacesBookmarkRemoved : PlacesBookmark {
  constructor(PlacesBookmarkRemovedInit initDict);
  /**
   * The item's index in the folder.
   */
  readonly attribute long index;

  /**
   * The item is a descendant of an item whose notification has been sent out.
   */
  readonly attribute boolean isDescendantRemoval;
};

dictionary PlacesBookmarkMovedInit {
  required long long id;
  required unsigned short itemType;
  DOMString? url = null;
  required ByteString guid;
  required ByteString parentGuid;
  required long index;
  required ByteString oldParentGuid;
  required long oldIndex;
  required unsigned short source;
  required boolean isTagging;
};

[ChromeOnly, Exposed=Window]
/**
 * NOTE: Though PlacesBookmarkMoved extends PlacesBookmark,
 *       parentId will not be set. Use parentGuid instead.
 */
interface PlacesBookmarkMoved : PlacesBookmark {
  constructor(PlacesBookmarkMovedInit initDict);
  /**
   * The item's index in the folder.
   */
  readonly attribute long index;

  /**
   * The unique ID associated with the item's old parent.
   */
  readonly attribute ByteString oldParentGuid;

  /**
   * The item's old index in the folder.
   */
  readonly attribute long oldIndex;
};

dictionary PlacesFaviconInit {
  required DOMString url;
  required ByteString pageGuid;
  required DOMString faviconUrl;
};

[ChromeOnly, Exposed=Window]
interface PlacesFavicon : PlacesEvent {
  constructor(PlacesFaviconInit initDict);

  /**
   * The URI of the changed page.
   */
  readonly attribute DOMString url;

  /**
   * The unique id associated with the page.
   */
  readonly attribute ByteString pageGuid;

  /**
   * The URI of the new favicon.
   */
  readonly attribute DOMString faviconUrl;
};

dictionary PlacesVisitTitleInit {
  required DOMString url;
  required ByteString pageGuid;
  required DOMString title;
};

[ChromeOnly, Exposed=Window]
interface PlacesVisitTitle : PlacesEvent {
  constructor(PlacesVisitTitleInit initDict);

  /**
   * The URI of the changed page.
   */
  readonly attribute DOMString url;

  /**
   * The unique id associated with the page.
   */
  readonly attribute ByteString pageGuid;

  /**
   * The title of the changed page.
   */
  readonly attribute DOMString title;
};

[ChromeOnly, Exposed=Window]
interface PlacesHistoryCleared : PlacesEvent {
  constructor();
};

[ChromeOnly, Exposed=Window]
interface PlacesRanking : PlacesEvent {
  constructor();
};

dictionary PlacesVisitRemovedInit {
  required DOMString url;
  required ByteString pageGuid;
  required unsigned short reason;
  unsigned long transitionType = 0;
  boolean isRemovedFromStore = false;
  boolean isPartialVisistsRemoval = false;
};

[ChromeOnly, Exposed=Window]
interface PlacesVisitRemoved : PlacesEvent {
  /**
   * Removed by the user.
   */
  const unsigned short REASON_DELETED = 0;

  /**
   * Removed by periodic expiration.
   */
  const unsigned short REASON_EXPIRED = 1;

  constructor(PlacesVisitRemovedInit initDict);

  /**
   * The URI of the page.
   */
  readonly attribute DOMString url;

  /**
   * The unique id associated with the page.
   */
  readonly attribute ByteString pageGuid;

  /**
   * The reason of removal.
   */
  readonly attribute unsigned short reason;

  /**
   * One of nsINavHistoryService.TRANSITION_*
   * NOTE: Please refer this attribute only when isRemovedFromStore is false.
   *       Otherwise this will be 0.
   */
  readonly attribute unsigned long transitionType;

  /**
   * This will be true if the page is removed from store.
   * If false, only visits were removed, but not the page.
   */
  readonly attribute boolean isRemovedFromStore;

  /**
   * This will be true if remains at least one visit to the page.
   */
  readonly attribute boolean isPartialVisistsRemoval;
};

[ChromeOnly, Exposed=Window]
interface PlacesPurgeCaches : PlacesEvent {
  constructor();
};
