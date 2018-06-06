# Metrics we collect

By default, the about:newtab and about:home pages in Firefox (the pages you see when you open a new tab and when you start the browser), will send data back to Mozilla servers about usage of these pages.  The intent is to collect data in order to improve the user's experience while using Activity Stream.  Data about your specific browsing behaior or the sites you visit is **never transmitted to any Mozilla server**.  At any time, it is easy to **turn off** this data collection by [opting out of Firefox telemetry](https://support.mozilla.org/kb/share-telemetry-data-mozilla-help-improve-firefox).

Data is sent to our servers in the form of discreet HTTPS 'pings' or messages whenever you do some action on the Activity Stream about:home, about:newtab or about:welcome pages.  We try to minimize the amount and frequency of pings by batching them together.  Pings are sent in [JSON serialized format](http://www.json.org/).

At Mozilla, [we take your privacy very seriously](https://www.mozilla.org/privacy/).  The Activity Stream page will never send any data that could personally identify you.  We do not transmit what you are browsing, searches you perform or any private settings.  Activity Stream does not set or send cookies, and uses [Transport Layer Security](https://en.wikipedia.org/wiki/Transport_Layer_Security) to securely transmit data to Mozilla servers.

Data collected from Activity Stream is retained on Mozilla secured servers for a period of 30 days before being rolled up into an anonymous aggregated format.  After this period the raw data is deleted permanently.  Mozilla **never shares data with any third party**.

The following is a detailed overview of the different kinds of data we collect in the Activity Stream. See [data_dictionary.md](data_dictionary.md) for more details for each field.

## Health ping

This is a heartbeat ping indicating whether Activity Stream is currently being used or not, it's submitted once upon the browser initialization.

```js
{
  "client_id": "374dc4d8-0cb2-4ac5-a3cf-c5a9bc3c602e",
  "locale": "en-US",
  "version": "62.0a1",
  "release_channel": "nightly",
  "event": "AS_ENABLED",
  "value": 10
}
```
where the "value" is encoded as:
  * Value 0: default
  * Value 1: about:blank
  * Value 2: web extension
  * Value 3: other custom URL(s)
Two encoded integers for about:newtab and about:home are combined in a bitwise fashion. For instance, if both about:home and about:newtab were set to about:blank, then `value = 5 = (1 | (1 << 2))`, i.e `value = (bitfield of about:newtab) | (bitfield of about:newhome << 2)`.

## User event pings

These pings are captured when a user **performs some kind of interaction** in the add-on.

### Basic shape

A user event ping includes some basic metadata (tab id, addon version, etc.) as well as variable fields which indicate the location and action of the event.

```js
{
  // This indicates the type of interaction
  "event": ["CLICK", "SEARCH", "BLOCK", "DELETE", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "BOOKMARK_DELETE", "BOOKMARK_ADD", "OPEN_NEWTAB_PREFS", "CLOSE_NEWTAB_PREFS"],

  // Optional field indicating the UI component type
  "source": "TOP_SITES",

  // Optional field if there is more than one of a component type on a page.
  // It is zero-indexed.
  // For example, clicking the second Highlight would result in an action_position of 1
  "action_position": 1,

  // Basic metadata
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown" ],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "action": "activity_stream_event",
  "user_prefs": 7
}
```

### Types of user interactions

#### Performing a search

```js
{
  "event": "SEARCH",

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Clicking a top site item

```js
{
  "event": "CLICK",
  "source": "TOP_SITES",
  "action_position": 2,
  "value": {
    "card_type": "pinned",
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Deleting an item from history

```js
{
  "event": "DELETE",
  "source": "TOP_SITES",
  "action_position": 2,
  "value": {
    "card_type": "pinned",
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Blocking a site

```js
{
  "event": "BLOCK",
  "source": "TOP_SITES",
  "action_position": 2,
  "value": {
    "card_type": "pinned",
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Bookmarking a link

```js
{
  "event": "BOOKMARK_ADD",
  "source": "HIGHLIGHTS",
  "action_position": 2,
  "value": {
    "card_type": "trending"
  }
  
  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Removing a bookmark from a link

```js
{
  "event": "BOOKMARK_DELETE",
  "source": "HIGHLIGHTS",
  "action_position": 2,
  "value": {
    "card_type": "bookmark"
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Opening a link in a new window

```js
{
  "event": "OPEN_NEW_WINDOW",
  "source": "TOP_SITES",
  "action_position": 2,
  "value": {
    "card_type": "pinned",
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Opening a link in a new private window

```js
{
  "event": "OPEN_PRIVATE_WINDOW",
  "source": "TOP_SITES",
  "action_position": 2,
  "value": {
    "card_type": "pinned",
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Opening the new tab preferences pane

```js
{
  "event": "OPEN_NEWTAB_PREFS",

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Closing the new tab preferences pane

```js
{
  "event": "CLOSE_NEWTAB_PREFS",

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Acknowledging a section disclaimer

```js
{
  "event": "SECTION_DISCLAIMER_ACKNOWLEDGED",
  "source": "TOP_STORIES",

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Adding or editing a new TopSite

```js
{
  "event": "TOP_SITES_EDIT",
  "source": "TOP_SITES_SOURCE",
  // "-1" Is used for prepending a new TopSite at the front of the list, while
  // any other possible value is used for editing an existing TopSite slot.
  "action_position": [-1 | "0..TOP_SITES_LENGTH"]
}
```

#### Requesting a custom screenshot preview

```js
{
  "event": "PREVIEW_REQUEST",
  "source": "TOP_SITES"
}
```

## Session end pings

When a session ends, the browser will send a `"activity_stream_session"` ping to our metrics servers. This ping contains the length of the session, a unique reason for why the session ended, and some additional metadata.

### Basic event

All `"activity_stream_session"` pings have the following basic shape. Some fields are variable.

```js
{
  "action": "activity_stream_session",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "session_duration": 4199,
  "region": "US",
  "profile_creation_date": 14786,
  "user_prefs": 7
}
```

### What causes a session end?

Here are different scenarios that cause a session end event to be sent:

1. After a search
2. Clicking on something that causes navigation (top site, highlight, etc.)
3. Closing the browser
5. Refreshing
6. Navigating to a new URL via the url bar or file menu


### Session performance data

This data is held in a child object of the `activity_stream_session` event called `perf`.  All fields suffixed by `_ts` are type `DOMHighResTimeStamp` (aka a double of milliseconds, with a 5 microsecond precision) with 0 being the [timeOrigin](https://developer.mozilla.org/en-US/docs/Web/API/DOMHighResTimeStamp#The_time_origin) of the browser's hidden chrome window.

An example might look like this:

```javascript
perf: {
  // Timestamp of the action perceived by the user to trigger the load
  // of this page.
  //
  // Not required at least for error cases where the
  // observer event doesn't fire
  "load_trigger_ts": 1,

  // What was the perceived trigger of the load action:
  "load_trigger_type": [
    "first_window_opened" | // home only
    "menu_plus_or_keyboard" | // newtab only
    "unexpected" // sessions lacking actual start times
  ],

  // when the page itself receives an event that document.visibilityStat=visible
  "visibility_event_rcvd_ts": 2,

  // When did the topsites element finish painting?  Note that, at least for
  // the first tab to be loaded, and maybe some others, this will be before
  // topsites has yet to receive screenshots updates from the add-on code,
  // and is therefore just showing placeholder screenshots.
  "topsites_first_painted_ts": 5,

  // The 6 different types of TopSites icons and how many of each kind did the
  // user see.
  "topsites_icon_stats": {
    "custom_screenshot": 0,
    "screenshot_with_icon": 2,
    "screenshot": 1,
    "tippytop": 2,
    "rich_icon": 1,
    "no_image": 0
  },

  // The number of Top Sites that are pinned.
  "topsites_pinned": 3,

  // How much longer the data took, in milliseconds, to be ready for display
  // than it would have been in the ideal case. The user currently sees placeholder
  // cards instead of real cards for approximately this length of time. This is
  // sent when the first call of the component's `render()` method happens with
  // `this.props.initialized` set to `false`, and the value is the amount of
  // time in ms until `render()` is called with `this.props.initialized` set to `true`.
  "highlights_data_late_by_ms": 67,
  "topsites_data_late_by_ms": 35,

  // Whether the page is preloaded or not.
  "is_preloaded": [true|false],

  // Whether the page is prerendered or not.
  "is_prerendered": [true|false]
}
```

## Top Story pings

When Top Story (currently powered by Pocket) is enabled in Activity Stream, the browser will send following `activity_stream_impression_stats` to our metrics servers.

### Impression stats

This reports all the Pocket recommended articles (a list of `id`s) when the user opens a newtab.

```js
{
  "action": "activity_stream_impression_stats",

  // both "client_id" and "session_id" are set to "n/a" in this ping.
  "client_id": "n/a",
  "session_id": "n/a",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "source": "pocket",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "user_prefs": 7,
  "tiles": [{"id": 10000}, {"id": 10001}, {"id": 10002}]
}
```

### Click/block/save_to_pocket ping

This reports the user's interaction with those Pocket tiles.

```js
{
  "action": "activity_stream_impression_stats",

  // both "client_id" and "session_id" are set to "n/a" in this ping.
  "client_id": "n/a",
  "session_id": "n/a",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "source": "pocket",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "user_prefs": 7,

  // "pos" is the 0-based index to record the tile's position in the Pocket section.
  "tiles": [{"id": 10000, "pos": 0}],

  // A 0-based index to record which tile in the "tiles" list that the user just interacted with.
  "click|block|pocket": 0
}
```

## Performance pings

These pings are captured to record performance related events i.e. how long certain operations take to execute.

### Domain affinity calculation

This reports the duration of the domain affinity calculation in milliseconds.

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7,
  "event": "topstories.domain.affinity.calculation.ms",
  "value": 43
}
```

## Undesired event pings

These pings record the undesired events happen in the addon for further investigation.

### Addon initialization failure

This reports when the addon fails to initialize

```js
{
  "action": "activity_stream_undesired_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "1.0.12",
  "locale": "en-US",
  "user_prefs": 7,
  "event": "ADDON_INIT_FAILED",
  "value": -1
}
```
## Activity Stream Router pings

These pings record the impression and user interactions within Activity Stream Router.

### Impression ping

This reports the impression of Activity Stream Router.

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


### User interaction pings

This reports the user's interaction with Activity Stream Router.

```js
{
  "client_id": "n/a",
  "action": ["snippets_user_event" | "onboarding_user_event"],
  "addon_version": "1.0.12",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "locale": "en-US",
  "source": "NEWTAB_FOOTER_BAR",
  "message_id": "some_snippet_id",
  "event": ["CLICK_BUTTION" | "BLOCK"]
}
```
