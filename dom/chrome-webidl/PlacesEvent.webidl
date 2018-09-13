enum PlacesEventType {
  "none",

  /**
   * data: PlacesVisit. Fired whenever a page is visited.
   */
  "page-visited",
};

[ChromeOnly, Exposed=(Window,System)]
interface PlacesEvent {
  readonly attribute PlacesEventType type;
};

[ChromeOnly, Exposed=(Window,System)]
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
