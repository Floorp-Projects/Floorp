# Activity Stream Pings

The Activity Stream system add-on sends various types of pings to the backend (HTTPS POST) [Onyx server](https://github.com/mozilla/onyx) :
- a `health` ping that reports whether or not a user has a custom about:home or about:newtab page
- a `session` ping that describes the ending of an Activity Stream session (a new tab is closed or refreshed), and
- an `event` ping that records specific data about individual user interactions while interacting with Activity Stream
- an `impression_stats` ping that records data about Pocket impressions and user interactions

Schema definitions/validations that can be used for tests can be found in `system-addon/test/schemas/pings.js`.

## Example Activity Stream `health` log

```js
{
  "addon_version": "20180710100040",
  "client_id": "374dc4d8-0cb2-4ac5-a3cf-c5a9bc3c602e",
  "locale": "en-US",
  "version": "62.0a1",
  "release_channel": "nightly",
  "event": "AS_ENABLED",
  "value": 10,

  // These fields are generated on the server
  "date": "2016-03-07",
  "ip": "10.192.171.13",
  "ua": "python-requests/2.9.1",
  "receive_at": 1457396660000
}
```

## Example Activity Stream `session` Log

```js
{
  // These fields are sent from the client
  "action": "activity_stream_session",
  "addon_version": "20180710100040",
  "client_id": "374dc4d8-0cb2-4ac5-a3cf-c5a9bc3c602e",
  "locale": "en-US",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "session_duration": 1635,
  "session_id": "{12dasd-213asda-213dkakj}",
  "profile_creation_date": 14786,
  "user_prefs": 7

  // These fields are generated on the server
  "date": "2016-03-07",
  "ip": "10.192.171.13",
  "ua": "python-requests/2.9.1",
  "receive_at": 1457396660000
}
```

## Example Activity Stream `user_event` Log

```js
{
  "action": "activity_stream_user_event",
  "action_position": "3",
  "addon_version": "20180710100040",
  "client_id": "374dc4d8-0cb2-4ac5-a3cf-c5a9bc3c602e",
  "event": "click or scroll or search or delete",
  "locale": "en-US",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "source": "top sites, or bookmarks, or...",
  "session_id": "{12dasd-213asda-213dkakj}",
  "recommender_type": "pocket-trending",
  "metadata_source": "MetadataService or Local or TippyTopProvider",
  "user_prefs": 7

  // These fields are generated on the server
  "ip": "10.192.171.13",
  "ua": "python-requests/2.9.1",
  "receive_at": 1457396660000,
  "date": "2016-03-07",
}
```

## Example Activity Stream `impression_stats` Logs

```js
{
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "source": "pocket",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"]
  "tiles": [{"id": 10000}, {"id": 10001}, {"id": 10002}]
  "user_prefs": 7,
  "window_inner_width": 1000,
  "window_inner_height" 900
}
```

```js
{
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "source": "pocket",
  "page": "unknown",
  "user_prefs": 7,
  "window_inner_width": 1000,
  "window_inner_height" 900,

  // "pos" is the 0-based index to record the tile's position in the Pocket section.
  // "shim" is a base64 encoded shim attached to spocs, unique to the impression from the Ad server.
  "tiles": [{"id": 10000, "pos": 0, "shim": "enuYa1j73z3RzxgTexHNxYPC/b,9JT6w5KB0CRKYEU+"}],

  // A 0-based index to record which tile in the "tiles" list that the user just interacted with.
  "click|block|pocket": 0
}
```

## Example Activity Stream `Router` Pings

```js
{
  "client_id": "n/a",
  "action": ["snippets_user_event" | "onboarding_user_event"],
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "source": "pocket",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "source": "NEWTAB_FOOTER_BAR",
  "message_id": "some_snippet_id",
  "event": "IMPRESSION",
  "event_context": "{\"view\":\"application_menu\"}"
}
```

```eval_rst
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| KEY                        | DESCRIPTION                                                                                                                                          |                  |
+============================+======================================================================================================================================================+==================+
| ``action_position``        | [Optional] The index of the element in the ``source`` that was clicked.                                                                              | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``action``                 | [Required] Either ``activity_stream_event`` or ``activity_stream_session``.                                                                          | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``addon_version``          | [Required] Firefox build ID, i.e. ``Services.appinfo.appBuildID``.                                                                                   | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``advertiser``             | [Optional] An identifier for the advertiser used by the sponsored TopSites telemetry pings.                                                          | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``client_id``              | [Required] An identifier for this client.                                                                                                            | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``card_type``              | [Optional] ("bookmark", "pocket", "trending", "pinned", "search", "spoc", "organic")                                                                 | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``context_id``             | [Optional] An identifier used by the sponsored TopSites telemetry pings.                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``search_vendor``          | [Optional] the vendor of the search shortcut, one of ("google", "amazon", "wikipedia", "duckduckgo", "bing", etc.). This field only exists when      |                  |
|                            | ``card_type = "search"``                                                                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``date``                   | [Auto populated by Onyx] The date in YYYY-MM-DD format.                                                                                              | :three:          |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``shield_id``              | [Optional] DEPRECATED: use `experiments` instead. The unique identifier for a specific experiment.                                                   | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``event``                  | [Required] The type of event. Any user defined string ("click", "share", "delete", "more\_items")                                                    | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``event_context``          | [Optional] A string to record the context of an AS Router event ping. Compound context values will be stringified by JSON.stringify                  | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``highlight_type``         | [Optional] Either ["bookmarks", "recommendation", "history"].                                                                                        | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``impression_id``          | [Optional] The unique impression identifier for a specific client.                                                                                   | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``ip``                     | [Auto populated by Onyx] The IP address of the client.                                                                                               | :two:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``locale``                 | [Auto populated by Onyx] The browser chrome's language (eg. en-US).                                                                                  | :two:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``load_trigger_ts``        | [Optional][Server Counter][Server Alert for too many omissions] DOMHighResTimeStamp of the action perceived by the user to trigger the load of this  |                  |
|                            | page.                                                                                                                                                | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``load_trigger_type``      | [Server Counter][Server Alert for too many omissions] Either ["first\_window\_opened", "menu\_plus\_or\_keyboard", "unexpected"].                    | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``metadata_source``        | [Optional] The source of which we computed metadata. Either (``MetadataService`` or ``Local`` or ``TippyTopProvider``).                              | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``page``                   | [Required] One of ["about:newtab", "about:home", "about:welcome", "unknown" (which either means not-applicable or is a bug)].                        | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``position``               | [Optional] An integer indicating the placement (1-based) of the sponsored TopSites tile.                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``recommender_type``       | [Optional] The type of recommendation that is being shown, if any.                                                                                   | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``reporting_url``          | [Optional] A URL used by Mozilla services for impression and click reporting for the sponsored TopSites.                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``session_duration``       | [Optional][Server Counter][Server Alert for too many omissions] Time in (integer) milliseconds of the difference between the new tab becoming visible| :one:            |
|                            | and losing focus                                                                                                                                     |                  |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``session_id``             | [Optional] The unique identifier for a specific session.                                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``source`` (AS)            | [Required] Either ("recent\_links", "recent\_bookmarks", "frecent\_links", "top\_sites", "spotlight", "sidebar") and indicates what ``action``.      | :two:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``source`` (CS)            | [Optional] Either "newtab" or "urlbar" indicating the location of the TopSites pings for Contextual Services.                                        | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``received_at``            | [Auto populated by Onyx] The time in ms since epoch.                                                                                                 | :three:          |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``tile_id``                | [Optional] An integer identifier for a sponsored TopSites tile.                                                                                      | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``total_bookmarks``        | [Optional] The total number of bookmarks in the user's places db.                                                                                    | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``total_history_size``     | [Optional] The number of history items currently in the user's places db.                                                                            | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``ua``                     | [Auto populated by Onyx] The user agent string.                                                                                                      | :two:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``unload_reason``          | [Required] The reason the Activity Stream page lost focus.                                                                                           | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``url``                    | [Optional] The URL of the recommendation shown in one of the highlights spots, if any.                                                               | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``value`` (event)          | [Optional] An object with keys "icon\_type", "card\_type" and "pocket\_logged\_in\_status" to record the extra information for event ping            | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``ver``                    | [Auto populated by Onyx] The version of the Onyx API the ping was sent to.                                                                           | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``highlights_size``        | [Optional] The size of the Highlights set.                                                                                                           | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| highlights_data_late_by_ms | [Optional] Time in ms it took for Highlights to become initialized                                                                                   | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| topsites_data_late_by_ms   | [Optional] Time in ms it took for TopSites to become initialized                                                                                     | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| topsites_first_painted_ts  | [Optional][Service Counter][Server Alert for too many omissions] Timestamp of when the Top Sites element finished painting (possibly with only       |                  |
|                            | placeholder screenshots)                                                                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``custom_screenshot``      | [Optional] Number of topsites that display a custom screenshot.                                                                                      | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``screenshot_with_icon``   | [Optional] Number of topsites that display a screenshot and a favicon.                                                                               | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``screenshot``             | [Optional] Number of topsites that display only a screenshot.                                                                                        | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``tippytop``               | [Optional] Number of topsites that display a tippytop icon.                                                                                          | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``rich_icon``              | [Optional] Number of topsites that display a high quality favicon.                                                                                   | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``no_image``               | [Optional] Number of topsites that have no screenshot.                                                                                               | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``topsites_pinned``        | [Optional] Number of topsites that are pinned.                                                                                                       | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| topsites_search_shortcuts  | [Optional] Number of search shortcut topsites.                                                                                                       | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| visibility_event_rcvd_ts   | [Optional][Server Counter][Server Alert for too many omissions] DOMHighResTimeStamp of when the page itself receives an event that                   |                  |
|                            | document.visibilityState == visible.                                                                                                                 | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``tiles``                  | [Required] A list of tile objects for the Pocket articles. Each tile object mush have a ID, optionally a "pos" property to indicate the tile         |                  |
|                            | position, and optionally a "shim" property unique to the impression from the Ad server                                                               | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``click``                  | [Optional] An integer to record the 0-based index when user clicks on a Pocket tile.                                                                 | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``block``                  | [Optional] An integer to record the 0-based index when user blocks a Pocket tile.                                                                    | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``pocket``                 | [Optional] An integer to record the 0-based index when user saves a Pocket tile to Pocket.                                                           | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``window_inner_width``     | [Optional] Amount of vertical space in pixels available to the window.                                                                               | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``window_inner_height``    | [Optional] Amount of horizontal space in pixels available to the window.                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``user_prefs``             | [Required] The encoded integer of user's preferences.                                                                                                | :one: & :four:   |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``is_preloaded``           | [Required] A boolean to signify whether the page is preloaded or not                                                                                 | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``icon_type``              | [Optional] ("tippytop", "rich\_icon", "screenshot\_with\_icon", "screenshot", "no\_image", "custom\_screenshot")                                     | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``region``                 | [Optional] A string maps to pref "browser.search.region", which is essentially the two letter ISO 3166-1 country code populated by the Firefox       |                  |
|                            | search service. Note that: 1). it reports "OTHER" for those regions with smaller Firefox user base (less than 10000) so that users cannot be         |                  |
|                            | uniquely identified; 2). it reports "UNSET" if this pref is missing; 3). it reports "EMPTY" if the value of this pref is an empty string.            | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``profile_creation_date``  | [Optional] An integer to record the age of the Firefox profile as the total number of days since the UNIX epoch.                                     | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``message_id``             | [required] A string identifier of the message in Activity Stream Router.                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``has_flow_params``        | [required] One of [true, false]. A boolean identifier that indicates if Firefox Accounts flow parameters are set or unset.                           | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``displayed``              | [required] 1: a SPOC is displayed; 0: non-displayed                                                                                                  | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``reason``                 | [required] The reason if a SPOC is not displayed, "n/a" for the displayed, one of ("frequency\_cap", "blocked\_by\_user", "flight\_duplicate",       |                  |
|                            | "probability\_selection", "below\_min\_score", "out\_of\_position", "n/a")                                                                           | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``full_recalc``            | [required] Is it a full SPOCS recalculation: 0: false; 1: true. Recalculation case: 1). fetch SPOCS from Pocket endpoint. Non-recalculation cases:   |                  |
|                            | 1). An impression updates the SPOCS; 2). Any action that triggers the ``selectLayoutRender``                                                         | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``experiments``            | [Optional] An object to record all active experiments (an empty object will be sent if there is no active experiment). The experiments IDs are       | :one:            |
|                            | stored as keys, and the value object stores the branch information.                                                                                  |                  |
|                            | `Example: {"experiment_1": {"branch": "control"}, "experiment_2": {"branch": "treatment"}}`. This deprecates the `shield_id` used in Activity Stream |                  |
|                            | and Messaging System.                                                                                                                                |                  |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``browser_session_id``     | [Optional] The unique identifier for a browser session, retrieved from TelemetrySession                                                              | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``attribution.source``     | [Optional] Referring partner domain, when install happens via a known partner                                                                        | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``attribution.medium``     | [Optional] Category of the source, such as 'organic' for a search engine                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``attribution.campaign``   | [Optional] Identifier of the particular campaign that led to the download of the product                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``attribution.content``    | [Optional] Identifier to indicate the particular link within a campaign                                                                              | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``attribution.experiment`` | [Optional] Funnel experiment identifier, see bug 1567339                                                                                             | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``attribution.variantion`` | [Optional] Funnel experiment variant identifier, see bug 1567339                                                                                     | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
| ``attribution.ua``         | [Optional] Derived user agent, see bug 1595063                                                                                                       | :one:            |
+----------------------------+------------------------------------------------------------------------------------------------------------------------------------------------------+------------------+
```

**Where:**

* :one: Firefox data
* :two: HTTP protocol data
* :three: server augmented data
* :four: User preferences encoding table


Note: the following session-related fields are not yet implemented in the system-addon,
but will likely be added in future versions:

```js
{
  "total_bookmarks": 19,
  "total_history_size": 9,
  "highlights_size": 20
}
```

## Encoding and decoding of `user_prefs`

This encoding mapping was defined in `system-addon/lib/TelemetryFeed.jsm`

```eval_rst
+-------------------+------------------------+
| Preference        | Encoded value (binary) |
+===================+========================+
| `showSearch`      | 1 (00000001)           |
+-------------------+------------------------+
| `showTopSites`    | 2 (00000010)           |
+-------------------+------------------------+
| `showTopStories`  | 4 (00000100)           |
+-------------------+------------------------+
| `showHighlights`  | 8 (00001000)           |
+-------------------+------------------------+
| `showSnippets`    | 16 (00010000)          |
+-------------------+------------------------+
| `showSponsored`   | 32 (00100000)          |
+-------------------+------------------------+
| `showCFRAddons`   | 64 (01000000)          |
+-------------------+------------------------+
| `showCFRFeatures` | 128 (10000000)         |
+-------------------+------------------------+
| `showSponsoredTopSites` | 256 (100000000)  |
+-------------------+------------------------+
```

Each item above could be combined with other items through bitwise OR (`|`) operation.

Examples:

* Everything is on, `user_prefs = 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 = 511`
* Everything is off, `user_prefs = 0`
* Only show search and Top Stories, `user_prefs = 1 | 4 = 5`
* Everything except Highlights, `user_prefs = 1 | 2 | 4 | 16 | 32 | 64 | 128 | 256 = 503`

Likewise, one can use bitwise AND (`&`) for decoding.

* Check if everything is shown, `user_prefs & (1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256)` or `user_prefs == 511`
* Check if everything is off, `user_prefs == 0`
* Check if search is shown, `user_prefs & 1`
* Check if both Top Sites and Top Stories are shown, `(user_prefs & 2) && (user_prefs & 4)`, or  `(user_prefs & (2 | 4)) == (2 | 4)`
