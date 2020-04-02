# Metrics we collect

By default, the about:newtab, about:welcome and about:home pages in Firefox (the pages you see when you open a new tab and when you start the browser), will send data back to Mozilla servers about usage of these pages.  The intent is to collect data in order to improve the user's experience while using Activity Stream.  Data about your specific browsing behaior or the sites you visit is **never transmitted to any Mozilla server**.  At any time, it is easy to **turn off** this data collection by [opting out of Firefox telemetry](https://support.mozilla.org/kb/share-telemetry-data-mozilla-help-improve-firefox).

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

## Page takeover ping

This ping is submitted once upon Activity Stream initialization if either about:home or about:newtab are set to a custom URL. It sends the category of the custom URL. It also includes the web extension id of the extension controlling the home and/or newtab page.

```js
{
  "event": "PAGE_TAKEOVER_DATA",
  "value": {
    "home_url_category": ["search-engine" | "search-engine-mozilla-tag" | "search-engine-other-tag" | "news-portal" | "ecommerce" | "social-media" | "known-hijacker" | "other"],
    "home_extension_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
    "newtab_url_category": ["search-engine" | "search-engine-mozilla-tag" | "search-engine-other-tag" | "news-portal" | "ecommerce" | "social-media" | "known-hijacker" | "other"],
    "newtab_extension_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  },

  // Basic metadata
  "action": "activity_stream_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7
}
```

## User event pings

These pings are captured when a user **performs some kind of interaction** in the add-on.

### Basic shape

A user event ping includes some basic metadata (tab id, addon version, etc.) as well as variable fields which indicate the location and action of the event.

```js
{
  // This indicates the type of interaction
  "event": ["CLICK", "SEARCH", "BLOCK", "DELETE", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "BOOKMARK_DELETE", "BOOKMARK_ADD", "OPEN_NEWTAB_PREFS", "CLOSE_NEWTAB_PREFS", "SEARCH_HANDOFF"],

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
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "action": "activity_stream_event",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
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
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Performing a search handoff

```js
{
  "event": "SEARCH_HANDOFF",

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
    "card_type": ["pinned" | "search" | "spoc"],
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image" | "custom_screenshot"],
    // only exists if its card_type = "search"
    "search_vendor": "google"
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Clicking a top story item

```js
{
  "event": "CLICK",
  "source": "CARDGRID",
  "action_position": 2,
  "value": {
    // "spoc" for sponsored stories, "organic" for regular stories.
    "card_type": ["organic" | "spoc"],
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Adding a search shortcut
```js
{
  "event": "SEARCH_EDIT_ADD",
  "source": "TOP_SITES",
  "action_position": 2,
  "value": {
    "search_vendor": "google"
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Showing privacy information

```js
{
  "event": "SHOW_PRIVACY_INFO",
  "source": "TOP_SITES",
  "action_position": 2,

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Clicking on privacy information link

```js
{
  "event": "CLICK_PRIVACY_INFO",
  "source": "DS_PRIVACY_MODAL",

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Deleting a search shortcut
```js
{
  "event": "SEARCH_EDIT_DELETE",
  "source": "TOP_SITES",
  "action_position": 2,
  "value": {
    "search_vendor": "google"
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image" | "custom_screenshot"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
    "card_type": ["pinned" | "search" | "spoc"],
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image" | "custom_screenshot"],
    // only exists if its card_type = "search"
    "search_vendor": "google"
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image" | "custom_screenshot"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
    "icon_type": ["screenshot_with_icon" | "screenshot" | "tippytop" | "rich_icon" | "no_image" | "custom_screenshot"]
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
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
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "user_prefs": 7
}
```

#### Pinning a tab

```js
{
  "event": "TABPINNED",
  "source": "TAB_CONTEXT_MENU",
  "value": "{\"total_pinned_tabs\":2}",

  // Basic metadata
  "action": "activity_stream_user_event",
  "client_id": "aabaace5-35f4-7345-a28e-5502147dc93c",
  "version": "67.0a1",
  "addon_version": "20190218094427",
  "locale": "en-US",
  "user_prefs": 59,
  "page": "n/a",
  "session_id": "n/a",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3"
}
```

#### Adding or editing a new TopSite

```js
{
  "event": "TOP_SITES_EDIT",
  "source": "TOP_SITES_SOURCE",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  // "-1" Is used for prepending a new TopSite at the front of the list, while
  // any other possible value is used for editing an existing TopSite slot.
  "action_position": [-1 | "0..TOP_SITES_LENGTH"]
}
```

#### Requesting a custom screenshot preview

```js
{
  "event": "PREVIEW_REQUEST",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "source": "TOP_SITES"
}
```

### Onboarding user events on about:welcome

#### Form Submit Events

```js
{
  "event": ["SUBMIT_EMAIL" | "SUBMIT_SIGNIN" | "SKIPPED_SIGNIN"],
  "value": {
    "has_flow_params": false,
  }

  // Basic metadata
  "action": "activity_stream_event",
  "page": "about:welcome",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7
}
```

#### Firefox Accounts Metrics flow errors

```js
{
  "event": ["FXA_METRICS_FETCH_ERROR" | "FXA_METRICS_ERROR"],
  "value": 500, // Only FXA_METRICS_FETCH_ERROR provides this value, this value is any valid HTTP status code except 200.

  // Basic metadata
  "action": "activity_stream_event",
  "page": "about:welcome",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "session_id": "005deed0-e3e4-4c02-a041-17405fd703f6",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7
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
  "addon_version": "20180710100040",
  "locale": "en-US",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "session_duration": 4199,
  "profile_creation_date": 14786,
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
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

  // The number of search shortcut Top Sites.
  "topsites_search_shortcuts": 2,

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
  "addon_version": "20180710100040",
  "locale": "en-US",
  "source": "pocket",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "user_prefs": 7,
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
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
  "addon_version": "20180710100040",
  "locale": "en-US",
  "source": "pocket",
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,

  // "pos" is the 0-based index to record the tile's position in the Pocket section.
  // "shim" is a base64 encoded shim attached to spocs, unique to the impression from the Ad server.
  "tiles": [{"id": 10000, "pos": 0, "shim": "enuYa1j73z3RzxgTexHNxYPC/b,9JT6w5KB0CRKYEU+"}],

  // A 0-based index to record which tile in the "tiles" list that the user just interacted with.
  "click|block|pocket": 0
}
```

## Performance pings

These pings are captured to record performance related events i.e. how long certain operations take to execute.

### Domain affinity calculation v1

This reports the duration of the domain affinity calculation in milliseconds.

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "topstories.domain.affinity.calculation.ms",
  "value": 43
}
```

### Domain affinity calculation v2

These report the duration of the domain affinity v2 calculations in milliseconds.

#### Total calculation in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_TOTAL_DURATION",
  "value": 43
}
```

#### getRecipe calculation in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_GET_RECIPE_DURATION",
  "value": 43
}
```

#### RecipeExecutor calculation in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_RECIPE_EXECUTOR_DURATION",
  "value": 43
}
```

#### taggers calculation in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_TAGGERS_DURATION",
  "value": 43
}
```

#### createInterestVector calculation in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_CREATE_INTEREST_VECTOR_DURATION",
  "value": 43
}
```

#### calculateItemRelevanceScore calculation in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_ITEM_RELEVANCE_SCORE_DURATION",
  "value": 43
}
```

### History size used for v2 calculation

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_HISTORY_SIZE",
  "value": 43
}
```

### Error events for v2 calculation

These report any failures during domain affinity v2 calculations, and where it failed.

#### getRecipe error

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_GET_RECIPE_ERROR"
}
```

#### generateRecipeExecutor error

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_GENERATE_RECIPE_EXECUTOR_ERROR"
}
```

#### createInterestVector error

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "PERSONALIZATION_V2_CREATE_INTEREST_VECTOR_ERROR"
}
```

### Discovery Stream loaded content

This reports all the loaded content (a list of `id`s and positions) when the user opens a newtab page and the page becomes visible. Note that this ping is a superset of the Discovery Stream impression ping, as impression pings are also subject to the individual visibility.

```js
{
  "action": "activity_stream_impression_stats",

  // Both "client_id" and "session_id" are set to "n/a" in this ping.
  "client_id": "n/a",
  "session_id": "n/a",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "source": ["HERO" | "CARDGRID" | "LIST"],
  "page": ["about:newtab" | "about:home" | "about:welcome" | "unknown"],
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,

  // Indicating this is a `loaded content` ping (as opposed to impression) as well as the size of `tiles`
  "loaded": 3,
  "tiles": [{"id": 10000, "pos": 0}, {"id": 10001, "pos": 1}, {"id": 10002, "pos": 2}]
}
```

### Discovery Stream performance pings

#### Request time of layout feed in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "LAYOUT_REQUEST_TIME",
  "value": 42
}
```

#### Request time of SPOCS feed in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "SPOCS_REQUEST_TIME",
  "value": 42
}
```

#### Request time of component feed feed in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "COMPONENT_FEED_REQUEST_TIME",
  "value": 42
}
```

#### Request time of total Discovery Stream feed in ms

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "DS_FEED_TOTAL_REQUEST_TIME",
  "value": 136
}
```

#### Cache age of Discovery Stream feed in second

```js
{
  "action": "activity_stream_performance_event",
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "DS_CACHE_AGE_IN_SEC",
  "value": 1800 // 30 minutes
}
```

### Discovery Stream SPOCS Fill ping

This reports the internal status of Pocket SPOCS (Sponsored Contents).

```js
{
  // both "client_id" and "session_id" are set to "n/a" in this ping.
  "client_id": "n/a",
  "session_id": "n/a",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "version": "68",
  "release_channel": "release",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "spoc_fills": [
    {"id": 10000, displayed: 0, reason: "frequency_cap", full_recalc: 1},
    {"id": 10001, displayed: 0, reason: "blocked_by_user", full_recalc: 1},
    {"id": 10002, displayed: 0, reason: "below_min_score", full_recalc: 1},
    {"id": 10003, displayed: 0, reason: "campaign_duplicate", full_recalc: 1},
    {"id": 10004, displayed: 0, reason: "probability_selection", full_recalc: 0},
    {"id": 10004, displayed: 0, reason: "out_of_position", full_recalc: 0},
    {"id": 10005, displayed: 1, reason: "n/a", full_recalc: 0}
  ]
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
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": "ADDON_INIT_FAILED",
  "value": -1
}
```

## Activity Stream Router pings

These pings record the impression and user interactions within Activity Stream Router.

### Impression ping

This reports the impression of Activity Stream Router.

#### Snippets impression
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "action": "snippets_user_event",
  "impression_id": "n/a",
  "source": "SNIPPETS",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "NEWTAB_FOOTER_BAR",
  "message_id": "some_snippet_id",
  "event": "IMPRESSION"
}
```

CFR impression ping has two forms, in which the message_id could be of different meanings.

#### CFR impression for all the prerelease channels and shield experiment
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "action": "cfr_user_event",
  "impression_id": "n/a",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "CFR",
  // message_id could be the ID of the recommendation, such as "wikipedia_addon"
  "message_id": "wikipedia_addon",
  "event": "IMPRESSION"
}
```

#### CFR impression for the release channel
```js
{
  "client_id": "n/a",
  "action": "cfr_user_event",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "CFR",
  // message_id should be a bucket ID in the release channel, we may not use the
  // individual ID, such as addon ID, per legal's request
  "message_id": "bucket_id",
  "event": "IMPRESSION"
}
```

#### Onboarding impression
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "action": "onboarding_user_event",
  "impression_id": "n/a",
  "source": "FIRST_RUN",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "message_id": "EXTENDED_TRIPLETS_1",
  "event": "IMPRESSION",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "event_context": { "page": ["about:welcome" | "about:home" | "about:newtab"] }
}
```

#### Onboarding Simplified Welcome impression
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "version": "76.0a1",
  "locale": "en-US",
  "experiments": {},
  "release_channel": "default",
  "addon_version": "20200330194034"
  "message_id": "SIMPLIFIED_ABOUT_WELCOME",
  "event": "IMPRESSION",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "event_context": { "page": "about:welcome" }
}
```
#### Onboarding Simplified Welcome Session End ping
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "version": "76.0a1",
  "locale": "en-US",
  "experiments": {},
  "release_channel": "default",
  "addon_version": "20200330194034"
  "message_id": "ABOUT_WELCOME_SESSION_END",
  "id": "ABOUT_WELCOME",
  "event": "SESSION_END",
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "event_context": { "page": "about:welcome", "reason":
    ["welcome-window-closed" | "welcome-tab-closed" | "app-shut-down" | "address-bar-navigated" | "unknown"]}
}
```

### User interaction pings

This reports the user's interaction with Activity Stream Router.

#### Snippets interaction pings
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "action": "snippets_user_event",
  "addon_version": "20180710100040",
  "impression_id": "n/a",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "NEWTAB_FOOTER_BAR",
  "message_id": "some_snippet_id",
  "event": ["CLICK_BUTTION" | "BLOCK"]
}
```

#### Onboarding interaction pings
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "action": "onboarding_user_event",
  "addon_version": "20180710100040",
  "impression_id": "n/a",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "ONBOARDING",
  "message_id": "onboarding_message_1",
  "event": ["IMPRESSION" | "CLICK_BUTTION" | "INSTALL" | "BLOCK"],
  "browser_session_id": "e7e52665-7db3-f348-9918-e93160eb2ef3",
  "event_context": { "page": ["about:welcome" | "about:home" | "about:newtab"] }
}
```

#### CFR interaction pings for all the prerelease channels and shield experiment
```js
{
  "client_id": "26288a14-5cc4-d14f-ae0a-bb01ef45be9c",
  "action": "cfr_user_event",
  "addon_version": "20180710100040",
  "impression_id": "n/a",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "CFR",
  // message_id could be the ID of the recommendation, such as "wikipedia_addon"
  "message_id": "wikipedia_addon",
  "event": "[IMPRESSION | INSTALL | PIN | BLOCK | DISMISS | RATIONALE | LEARN_MORE | CLICK | CLICK_DOORHANGER | MANAGE]",
  // "modelVersion" records the model identifier for the CFR machine learning experiment, see more detail in Bug 1594422.
  // Non-experiment users will not report this field.
  "event_context": "{ \"modelVersion\": \"some_model_version_id\" }"
}
```

#### CFR interaction pings for release channel
```js
{
  "client_id": "n/a",
  "action": "cfr_user_event",
  "addon_version": "20180710100040",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "CFR",
  // message_id should be a bucket ID in the release channel, we may not use the
  // individual ID, such as addon ID, per legal's request
  "message_id": "bucket_id",
  "event": "[IMPRESSION | INSTALL | PIN | BLOCK | DISMISS | RATIONALE | LEARN_MORE | CLICK | CLICK_DOORHANGER | MANAGE]"
}
```

### Targeting error pings

This reports when an error has occurred when parsing/evaluating a JEXL targeting string in a message.

```js
{
  "client_id": "n/a",
  "action": "asrouter_undesired_event",
  "addon_version": "20180710100040",
  "impression_id": "{005deed0-e3e4-4c02-a041-17405fd703f6}",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "message_id": "some_message_id",
  "event": "TARGETING_EXPRESSION_ERROR",
  "event_context": ["MALFORMED_EXPRESSION" | "OTHER_ERROR"]
}
```

### Remote Settings error pings

This reports a failure in the Remote Settings loader to load messages for Activity Stream Router.

```js
{
  "action": "asrouter_undesired_event",
  "client_id": "n/a",
  "addon_version": "20180710100040",
  "locale": "en-US",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "user_prefs": 7,
  "event": ["ASR_RS_NO_MESSAGES" | "ASR_RS_ERROR"],
  // The value is set to the ID of the message provider. For example: remote-cfr, remote-onboarding, etc.
  "event_context": "REMOTE_PROVIDER_ID"
}
```

## Trailhead experiment enrollment ping

This reports an enrollment ping when a user gets enrolled in a Trailhead experiment. Note that this ping is only collected through the Mozilla Events telemetry pipeline.

```js
{
  "category": "activity_stream",
  "method": "enroll",
  "object": "preference_study"
  "value": "activity-stream-firstup-trailhead-interrupts",
  "extra_keys": {
    "experimentType": "as-firstrun",
    "branch": ["supercharge" | "join" | "sync" | "privacy" ...]
  }
}
```

## Feature Callouts interaction pings

This reports when a user has seen or clicked a badge/notification in the browser toolbar in a non-PBM window

```
{
  "locale": "en-US",
  "client_id": "9da773d8-4356-f54f-b7cf-6134726bcf3d",
  "version": "70.0a1",
  "release_channel": "default",
  "addon_version": "20190712095934",
  "action": "cfr_user_event",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "CFR",
  "message_id": "FXA_ACCOUNTS_BADGE",
  "event": ["CLICK" | "IMPRESSION"],
}
```

## Panel interaction pings

This reports when a user opens the panel, views messages and clicks on a message.
For message impressions we concatenate the ids of all messages in the panel.

```
{
  "locale": "en-US",
  "client_id": "9da773d8-4356-f54f-b7cf-6134726bcf3d",
  "version": "70.0a1",
  "release_channel": "default",
  "addon_version": "20190712095934",
  "action": "cfr_user_event",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "source": "CFR",
  "message_id": "WHATS_NEW_70",
  "event": ["CLICK" | "IMPRESSION"],
  "value": { "view": ["application_menu" | "toolbar_dropdown"] }
}
```

## Moments page pings

This reports when a moments page message has set the user preference for
`browser.startup.homepage_override.once`. It goes through the same policy
as other CFR messages.

```
// Release ping
{
  "action": "cfr_user_event"
  "addon_version": "20200225022813"
  "bucket_id": "WNP_THANK_YOU"
  "event": "MOMENTS_PAGE_SET"
  "impression_id": "{d85d2268-b751-9543-b6d7-aad523bf2b26}"
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "locale": "en-US"
  "message_id": "n/a"
  "source": "CFR"
}

// Beta and Nightly channels
{
  "source": "CFR",
  "message_id": "WNP_THANK_YOU",
  "bucket_id": "WNP_THANK_YOU",
  "event": "MOMENTS_PAGE_SET",
  "addon_version": "20200225022813",
  "experiments": {
    "experiment_1": {"branch": "control"},
    "experiment_2": {"branch": "treatment"}
  },
  "locale": "en-US",
  "client_id": "21dc1375-b24e-984b-83e9-c8a9660ae4ff"
}
```
