# Activity Stream Pings

The Activity Stream system add-on sends various types of pings to the backend (HTTPS POST) [Onyx server](https://github.com/mozilla/onyx) :
- a `health` ping that reports whether or not a user has a custom about:home or about:newtab page
- a `session` ping that describes the ending of an Activity Stream session (a new tab is closed or refreshed), and
- an `event` ping that records specific data about individual user interactions while interacting with Activity Stream
- a `performance` ping that records specific performance related events
- an `undesired` ping that records data about bad app states and missing data
- an `impression_stats` ping that records data about Pocket impressions and user interactions

Schema definitions/validations that can be used for tests can be found in `system-addon/test/schemas/pings.js`.

# Example Activity Stream `health` log

```js
{
  "addon_version": "1.0.0",
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

# Example Activity Stream `session` Log

```js
{
  // These fields are sent from the client
  "action": "activity_stream_session",
  "addon_version": "1.0.0",
  "client_id": "374dc4d8-0cb2-4ac5-a3cf-c5a9bc3c602e",
  "locale": "en-US",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "session_duration": 1635,
  "session_id": "{12dasd-213asda-213dkakj}",
  "region": "US",
  "profile_creation_date": 14786,
  "user_prefs": 7

  // These fields are generated on the server
  "date": "2016-03-07",
  "ip": "10.192.171.13",
  "ua": "python-requests/2.9.1",
  "receive_at": 1457396660000
}
```

# Example Activity Stream `user_event` Log

```js
{
  "action": "activity_stream_user_event",
  "action_position": "3",
  "addon_version": "1.0.0",
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

# Example Activity Stream `performance` Log

```js
{
  "action": "activity_stream_performance_event",
  "addon_version": "1.0.0",
  "client_id": "374dc4d8-0cb2-4ac5-a3cf-c5a9bc3c602e",
  "event": "previewCacheHit",
  "event_id": "45f1912165ca4dfdb5c1c2337dbdc58f",
  "locale": "en-US",
  "page": "unknown", // all session-specific perf events should be part of the session perf object
  "receive_at": 1457396660000,
  "source": "TOP_FRECENT_SITES",
  "value": 1,
  "user_prefs": 7,

  // These fields are generated on the server
  "ip": "10.192.171.13",
  "ua": "python-requests/2.9.1",
  "receive_at": 1457396660000,
  "date": "2016-03-07"
}
```

# Example Activity Stream `undesired event` Log

```js
{
  "action": "activity_stream_undesired_event",
  "addon_version": "1.0.12",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "event": "MISSING_IMAGE",
  "locale": "en-US",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"]
  "source": "HIGHLIGHTS",
  "value": 0,
  "user_prefs": 7,

  // These fields are generated on the server
  "ip": "10.192.171.13",
  "ua": "python-requests/2.9.1",
  "receive_at": 1457396660000,
  "date": "2016-03-07"
}
```
# Example Activity Stream `impression_stats` Logs

```js
{
  "action": "activity_stream_impression_stats",
  "client_id": "n/a",
  "session_id": "n/a",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "source": "pocket",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"]
  "tiles": [{"id": 10000}, {"id": 10001}, {"id": 10002}]
  "user_prefs": 7
}
```

```js
{
  "action": "activity_stream_impression_stats",
  "client_id": "n/a",
  "session_id": "n/a",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "source": "pocket",
  "page": "unknown",
  "user_prefs": 7,

  // "pos" is the 0-based index to record the tile's position in the Pocket section.
  "tiles": [{"id": 10000, "pos": 0}],

  // A 0-based index to record which tile in the "tiles" list that the user just interacted with.
  "click|block|pocket": 0
}
```

# Example Activity Stream `Router` Pings

```js
{
  "client_id": "n/a",
  "action": ["snippets_user_event" | "onboarding_user_event"],
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "source": "pocket",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "source": "NEWTAB_FOOTER_BAR",
  "message_id": "some_snippet_id",
  "event": "IMPRESSION"
}
```

| KEY | DESCRIPTION | &nbsp; |
|-----|-------------|:-----:|
| `action_position` | [Optional] The index of the element in the `source` that was clicked. | :one:
| `action` | [Required] Either `activity_stream_event`, `activity_stream_session`, or `activity_stream_performance`. | :one:
| `addon_version` | [Required] The version of the Activity Stream addon. | :one:
| `client_id` | [Required] An identifier for this client. | :one:
| `card_type` | [Optional] ("bookmark", "pocket", "trending", "pinned") | :one:
| `date` | [Auto populated by Onyx] The date in YYYY-MM-DD format. | :three:
| `experiment_id` | [Optional] The unique identifier for a specific experiment. | :one:
| `event_id` | [Required] An identifier shared by multiple performance pings that describe ane entire request flow. | :one:
| `event` | [Required] The type of event. Any user defined string ("click", "share", "delete", "more_items") | :one:
| `highlight_type` | [Optional] Either ["bookmarks", "recommendation", "history"]. | :one:
| `impression_id` | [Optional] The unique impression identifier for a specific client. | :one:
| `ip` | [Auto populated by Onyx] The IP address of the client. | :two:
| `locale` | [Auto populated by Onyx] The browser chrome's language (eg. en-US). | :two:
| `load_trigger_ts` | [Optional][Server Counter][Server Alert for too many omissions]  DOMHighResTimeStamp of the action perceived by the user to trigger the load of this page. | :one:
| `load_trigger_type` | [Server Counter][Server Alert for too many omissions] Either ["first_window_opened", "menu_plus_or_keyboard", "unexpected"]. | :one:
| `metadata_source` | [Optional] The source of which we computed metadata. Either (`MetadataService` or `Local` or `TippyTopProvider`). | :one:
| `page` | [Required] One of ["about:newtab", "about:home", "about:welcome", "unknown" (which either means not-applicable or is a bug)]. | :one:
| `recommender_type` | [Optional] The type of recommendation that is being shown, if any. | :one:
| `session_duration` | [Optional][Server Counter][Server Alert for too many omissions] Time in (integer) milliseconds of the difference between the new tab becoming visible
and losing focus. | :one:
| `session_id` | [Optional] The unique identifier for a specific session. | :one:
| `source` | [Required] Either ("recent_links", "recent_bookmarks", "frecent_links", "top_sites", "spotlight", "sidebar") and indicates what `action`. | :two:
| `received_at` | [Auto populated by Onyx] The time in ms since epoch. | :three:
| `total_bookmarks` | [Optional] The total number of bookmarks in the user's places db. | :one:
| `total_history_size` | [Optional] The number of history items currently in the user's places db. | :one:
| `ua` | [Auto populated by Onyx] The user agent string. | :two:
| `unload_reason` | [Required] The reason the Activity Stream page lost focus. | :one:
| `url` | [Optional] The URL of the recommendation shown in one of the highlights spots, if any. | :one:
| `value` (performance) | [Required] An integer that represents the measured performance value. Can store counts, times in milliseconds, and should always be a positive integer.| :one:
| `value` (event) | [Optional] An object with keys "icon_type" and "card_type" to record the extra information for event ping| :one:
| `ver` | [Auto populated by Onyx] The version of the Onyx API the ping was sent to. | :one:
| `highlights_size` | [Optional] The size of the Highlights set. | :one:
| `highlights_data_late_by_ms` | [Optional] Time in ms it took for Highlights to become initialized | :one:
| `topsites_data_late_by_ms` | [Optional] Time in ms it took for TopSites to become initialized | :one:
| `topstories.domain.affinity.calculation.ms` | [Optional] Time in ms it took for domain affinities to be calculated | :one:
| `topsites_first_painted_ts` | [Optional][Service Counter][Server Alert for too many omissions] Timestamp of when the Top Sites element finished painting (possibly with only placeholder screenshots) | :one:
| `custom_screenshot` | [Optional] Number of topsites that display a custom screenshot. | :one:
| `screenshot_with_icon` | [Optional] Number of topsites that display a screenshot and a favicon. | :one:
| `screenshot` | [Optional] Number of topsites that display only a screenshot. | :one:
| `tippytop` | [Optional] Number of topsites that display a tippytop icon. | :one:
| `rich_icon` | [Optional] Number of topsites that display a high quality favicon. | :one:
| `no_image` | [Optional] Number of topsites that have no screenshot. | :one:
| `topsites_pinned` | [Optional] Number of topsites that are pinned. | :one:
| `visibility_event_rcvd_ts` | [Optional][Server Counter][Server Alert for too many omissions] DOMHighResTimeStamp of when the page itself receives an event that document.visibilityState == visible. | :one:
| `tiles` | [Required] A list of tile objects for the Pocket articles. Each tile object mush have a ID, and optionally a "pos" property to indicate the tile position | :one:
| `click` | [Optional] An integer to record the 0-based index when user clicks on a Pocket tile. | :one:
| `block` | [Optional] An integer to record the 0-based index when user blocks a Pocket tile. | :one:
| `pocket` | [Optional] An integer to record the 0-based index when user saves a Pocket tile to Pocket. | :one:
| `user_prefs` | [Required] The encoded integer of user's preferences. | :one: & :four:
| `is_prerendered` | [Required] A boolean to signify whether the page is prerendered or not | :one:
| `is_preloaded` | [Required] A boolean to signify whether the page is preloaded or not | :one:
| `icon_type` | [Optional] ("tippytop", "rich_icon", "screenshot_with_icon", "screenshot", "no_image") | :one:
| `region` | [Optional] A string maps to pref "browser.search.region", which is essentially the two letter ISO 3166-1 country code populated by the Firefox search service. Note that: 1). it reports "OTHER" for those regions with smaller Firefox user base (less than 10000) so that users cannot be uniquely identified; 2). it reports "UNSET" if this pref is missing; 3). it reports "EMPTY" if the value of this pref is an empty string. | :one:
| `profile_creation_date` | [Optional] An integer to record the age of the Firefox profile as the total number of days since the UNIX epoch. | :one:
|`message_id` | [required] A string identifier of the message in Activity Stream Router. | :one:

**Where:**

:one: Firefox data
:two: HTTP protocol data
:three: server augmented data
:four: User preferences encoding table


Note: the following session-related fields are not yet implemented in the system-addon,
but will likely be added in future versions:

```js
{
  "total_bookmarks": 19,
  "total_history_size": 9,
  "highlights_size": 20
}
```

This encoding mapping was defined in `system-addon/lib/TelemetryFeed.jsm`

| Preference | Encoded value |
| --- | --- |
| `showSearch` | 1 |
| `showTopSites` | 2 |
| `showTopStories` | 4 |
| `showHighlights` | 8 |
| `showSnippets`   | 16 |
| `showSponsored`  | 32 |

Each item above could be combined with other items through bitwise OR operation
