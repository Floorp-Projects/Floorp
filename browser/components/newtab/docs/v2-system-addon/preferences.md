# Preferences

## Preference branch

The preference branch for activity stream is `browser.newtabpage.activity-stream.`.
Any preferences defined in the preference configuration will be relative to that
branch. For example, if a preference is defined with the name `foo`, the full
preference as it is displayed in `about:config` will be `browser.newtabpage.activity-stream.foo`.

## Defining new preferences

All preferences for Activity Stream should be defined in the `PREFS_CONFIG` Array
found in `lib/ActivityStream.jsm`.
The configuration object should have a `name` (the name of the pref), a `title`
that describes the functionality of the pref, and a `value`, the default value
of the pref. Optionally a `getValue` function can be provided to dynamically
generate a default pref value based on args, e.g., geo and locale. For
developers-specific defaults, an optional `value_local_dev` will be used instead
of `value`. For example:

```js
{
  name: "telemetry.log",
  title: "Log telemetry events in the console",
  value: false,
  value_local_dev: true,
  getValue: ({geo}) => geo === "CA"
}
```

### IMPORTANT: Setting test-specific values for Mozilla Central

If a feed or feature behind a pref makes any network calls or would other be
disruptive for automated tests and that pref is on by default, make sure you
disable it for tests in Mozilla Central.

You should create a bug in Bugzilla and a patch that adds lines to turn off your
pref in the following files:
- layout/tools/reftest/reftest-preferences.js
- testing/profiles/prefs_general.js
- testing/talos/talos/config.py

You can see an example in [this patch](https://github.com/mozilla/activity-stream/pull/2977).

## Reading, setting, and observing preferences from `.jsm`s

To read/set/observe Activity Stream preferences, construct a `Prefs` instance found in `lib/ActivityStreamPrefs.jsm`.

```js
// Import Prefs
XPCOMUtils.defineLazyModuleGetter(this, "Prefs",
  "resource://activity-stream/lib/ActivityStreamPrefs.jsm");

// Create an instance
const prefs = new Prefs();
```

The `Prefs` utility will set the Activity Stream branch for you by default, so you
don't need to worry about prefixing every pref with `browser.newtabpage.activity-stream.`:

```js
const prefs = new Prefs();

// This will return the value of browser.newtabpage.activity-stream.foo
prefs.get("foo");

// This will set the value of browser.newtabpage.activity-stream.foo to true
prefs.set("foo", true)

// This will call aCallback when browser.newtabpage.activity-stream.foo is changed
prefs.observe("foo", aCallback);

// This will stop listening to browser.newtabpage.activity-stream.foo
prefs.ignore("foo", aCallback);
```

See [toolkit/modules/Preferences.jsm](https://searchfox.mozilla.org/mozilla-central/source/toolkit/modules/Preferences.jsm)
for more information about what methods are available.

## Discovery Stream Preferences

Preferences specific to the Discovery Stream are nested under the sub-branch `browser.newtabpage.activity-stream.discoverystream` (with the exception of `browser.newtabpage.blocked`).

#### `browser.newtabpage.activity-stream.discoverystream.flight.blocks`

- Type: `string (JSON)`
- Default: `{}`
- Pref Type: AS

Not intended for user configuration, but is programmatically updated. Used for tracking blocked flight IDs when a user dismisses a SPOC. Keys are flight IDs. Values don't have a specific meaning.

#### `browser.newtabpage.blocked`

- Type: `string (JSON)`
- Default: `null`
- Pref Type: AS

Not intended for user configuration, but is programmatically updated. Used for tracking blocked story IDs when a user dismisses one. Keys are story IDs. Values don't have a specific meaning.

#### `browser.newtabpage.activity-stream.discoverystream.config`

- Type `string (JSON)`
- Default:
  ```
  {
     "api_key_pref": "extensions.pocket.oAuthConsumerKey",
     "collapsible": true,
     "enabled": true,
     "show_spocs": true,
     "hardcoded_layout": true,
     "personalized": true,
     "layout_endpoint": "https://getpocket.cdn.mozilla.net/v3/newtab/layout?version=1&consumer_key=$apiKey&layout_variant=basic"
  }
  ```
  - `api_key_pref` (string): The name of a variable containing the key for the Pocket API.
  - `collapsible` (boolean): Controls whether the sections in new tab can be collapsed.
  - `enabled` (boolean): Controls whether DS is turned on and is programmatically set based on a user's locale. DS enablement is a logical `AND` of this and the value of `browser.newtabpage.activity-stream.discoverystream.enabled`.
  - `show_spocs` (boolean): Show sponsored content in new tab.
  - `hardcoded_layout` (boolean): When this is true, a hardcoded layout shipped with Firefox will be used instead of a remotely fetched layout definition.
  - `personalized` (boolean): When this is `true` personalized content based on browsing history will be displayed.
  - `layout_endpoint` (string): The URL for a remote layout definition that will be used if `hardcoded_layout` is `false`.
  - `unused_key` (string): This is not set by default and is unused by this codebase. It's a standardized way to differentiate configurations to prevent experiment participants from being unenrolled.

#### `browser.newtabpage.activity-stream.discoverystream.enabled`

- Type: `boolean`
- Default: `true`
- Pref Type: Firefox

When this is set to `true` the Discovery Stream experience will show up if `enabled` is also `true` on `browser.newtabpage.activity-stream.discoverystream.config`. Otherwise the old Activity Stream experience will be shown.

#### `browser.newtabpage.activity-stream.discoverystream.endpointSpocsClear`

- Type: `string (URL)`
- Default: `https://spocs.getpocket.com/user`
- Pref Type: AS

Endpoint for when a user opts-out of sponsored content to delete the corresponding data from the ad server.

#### `browser.newtabpage.activity-stream.discoverystream.endpoints`

- Type: `string (URLs, CSV)`
- Default: `https://getpocket.cdn.mozilla.net/,https://spocs.getpocket.com/`
- Pref Type: AS

A list of endpoints that are allowed to be used by Discovery Stream for remote content (eg: story metadata) and configuration (eg: remote layout definitions for experimentation).

#### `browser.newtabpage.activity-stream.discoverystream.engagementLabelEnabled`

- Type: `boolean`
- Default: `false`
- Pref Type: AS

A flag controlling the visibility of engagement labels on cards (eg: "Trending" or "Popular").

#### `browser.newtabpage.activity-stream.discoverystream.hardcoded-basic-layout`

- Type: `boolean`
- Default: `false`
- Pref Type: Firefox

If this is `false` the default hardcoded layout is used, and if it's `true` then an alternate hardcoded layout (that currently simulates the older AS experience) is used.

#### `browser.newtabpage.activity-stream.discoverystream.rec.impressions`

- Type: `string (JSON)`
- Default: `{}`
- Pref Type: AS

Programmatically generated hash table where the keys are recommendation IDs and the values are timestamps representing the first impression.

#### `browser.newtabpage.activity-stream.discoverystream.spoc.impressions`

- Type: `string (JSON)`
- Default: `{}`
- Pref Type: AS

Programmatically generated hash table where the keys are sponsored content IDs and the values are arrays of timestamps for every impression.

#### `browser.newtabpage.activity-stream.discoverystream.locale-list-config`

- Type: `string (CSV, locales)`
- Default: `null`
- Pref Type: Firefox

A comma separated list of locales that by default have stories enabled in newtab. It overrides what might be in region-stories-config. So if I set this to "en-US,en-CA,en-GB", all users with a English browser would see newtab stories, even if their region was not in region-stories-config list.

#### `browser.newtabpage.activity-stream.discoverystream.region-stories-config`

- Type: `string (CSV, regions)`
- Default: `US,DE,CA,GB,IE,CH,AT,BE`
- Pref Type: Firefox

A comma separated list of geos that by default have stories enabled in newtab. It matches the client's geo with that list, then looks for a matching locale.

#### `browser.newtabpage.activity-stream.discoverystream.region-spocs-config`

- Type: `string (CSV, regions)`
- Default: `US,CA,DE`
- Pref Type: Firefox

A comma separated list of geos that by default have spocs enabled in newtab. It matches the client's geo with that list.

#### `browser.newtabpage.activity-stream.discoverystream.region-layout-config`

- Type: `string (CSV, regions)`
- Default: `US,CA,GB,DE,IE,CH,AT,BE`
- Pref Type: Firefox

A comma separated list of geos that have 7 rows of stories enabled in newtab. It matches the client's geo with that list.

#### `browser.newtabpage.activity-stream.discoverystream.region-basic-layout`

- Type: `boolean`
- Default: false
- Pref Type: AS

If this is `true` newtabs with stories enabled see 1 row. It is set programmatically based on the result from region-layout-config.

#### `browser.newtabpage.activity-stream.discoverystream.spocs-endpoint`

- Type: `string (URL)`
- Default: `null`
- Pref Type: Firefox

Override to specify endpoint for SPOCs. Will take precedence over remote and hardcoded layout SPOC endpoints.

#### `browser.newtabpage.activity-stream.discoverystream.personalization.version`

- Type: `integer`
- Default: `1`
- Pref Type: Firefox

This controls what version of personalization we should use to score newtab stories.

#### `browser.newtabpage.activity-stream.discoverystream.personalization.modelKeys`

- Type: `string (CSV)`
- Default: `nb_model_arts_and_entertainment, nb_model_autos_and_vehicles, nb_model_beauty_and_fitness, nb_model_blogging_resources_and_services, nb_model_books_and_literature, nb_model_business_and_industrial, nb_model_computers_and_electronics, nb_model_finance, nb_model_food_and_drink, nb_model_games, nb_model_health, nb_model_hobbies_and_leisure, nb_model_home_and_garden, nb_model_internet_and_telecom, nb_model_jobs_and_education, nb_model_law_and_government, nb_model_online_communities, nb_model_people_and_society, nb_model_pets_and_animals, nb_model_real_estate, nb_model_reference, nb_model_science, nb_model_shopping, nb_model_sports, nb_model_travel`
- Pref Type: Firefox

This is a configuration for personalization version 2. It is a list of topics the algorithm uses to score stories by.

#### `browser.newtabpage.activity-stream.discoverystream.recs.personalized`

- Type: `boolean`
- Default: false
- Pref Type: Firefox

This controls if newtab story personalization includes regular stories or not. See spocs.personalized for sponsored content.

#### `browser.newtabpage.activity-stream.discoverystream.spocs.personalized`

- Type: `boolean`
- Default: true
- Pref Type: Firefox

This controls if newtab story personalization includes sponsored content or not. See recs.personalized for regular stories.

#### `browser.newtabpage.activity-stream.discoverystream.isCollectionDismissible`

- Type: `boolean`
- Default: true
- Pref Type: Firefox

This controls if newtab story collections are dismissible or not.

#### `browser.newtabpage.activity-stream.feeds.section.topstories`

- Type: `boolean`
- Default: true
- Pref Type: Firefox

This controls if the user should see newtab stories or not. It is set by the user via about:preferences#home

#### `browser.newtabpage.activity-stream.feeds.system.topstories`

- Type: `boolean`
- Default: false
- Pref Type: AS

Not intended for user configuration, but is programmatically set. It also controls if the user should see newtab stories or not. It is set at run time, and computed based on the locale/region.
