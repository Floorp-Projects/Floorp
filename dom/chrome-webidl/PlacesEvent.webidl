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
