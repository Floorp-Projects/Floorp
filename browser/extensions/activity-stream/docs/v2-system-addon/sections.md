# Sections in Activity Stream

Each section in Activity Stream displays data from a corresponding section feed
in a standardised `Section` UI component. Each section feed is responsible for
listening to events and updating the section options such as the title, icon,
and rows (the cards for the section to display).

The `Section` UI component displays the rows provided by the section feed. If no
rows are available it displays an empty state consisting of an icon and a
message. Optionally, the section may have a info option menu that is displayed
when users hover over the info icon.

On load, `SectionsManager` and `SectionsFeed` in `SectionsManager.jsm` add the
sections configured in the `BUILT_IN_SECTIONS` map to the state. These sections
are initially disabled, so aren't visible. The section's feed may use the
methods provided by the `SectionsManager` to enable its section and update its
properties.

The section configuration in `BUILT_IN_SECTIONS` consists of a generator
function keyed by the pref name for the section feed. The generator function
takes an `options` argument as the only parameter, which is passed the object
stored as serialised JSON in the pref `{feed_pref_name}.options`, or the empty
object if this doesn't exist. The generator returns a section configuration
object which may have the following properties:

Property | Type | Description
--- | --- | ---
id | String | Non-optional unique id.
title | Localisation object | Has property `id`, the string localisation id, and optionally a `values` object to fill in placeholders.
icon | String | Icon id. New icons should be added in icons.scss.
maxRows | Integer | Maximum number of rows of cards to display. Should be >= 1.
contextMenuOptions | Array of strings | The menu options to provide in the card context menus.
shouldHidePref | Boolean | If true, will the section preference in the preferences pane will not be shown.
pref | Object | Configures the section preference to show in the preferences pane. Has properties `titleString` and `descString`.
emptyState | Object | Configures the empty state of the section. Has properties `message` and `icon`.

## Section feeds

Each section feed should be controlled by the pref `feeds.section.{section_id}`.

### Enabling the section

The section feed must listen for the events `INIT` (dispatched when Activity
Stream is initialised) and `FEED_INIT` (dispatched when a feed is re-enabled
having been turned off, with the feed id as the `data`). On these events it must
call `SectionsManager.enableSection(id)`. Care should be taken that this happens
only once `SectionsManager` has also initialised; the feed can use the method
`SectionsManager.onceInitialized()`.

### Disabling the section

The section feed must have an `uninit` method. This is called when the section
feed is disabled by turning the section's pref off. In `uninit` the feed must
call `SectionsManager.disableSection(id)`. This will remove the section's UI
component from every existing Activity Stream page.

### Updating the section rows

The section feed can call `SectionsManager.updateSection(id, options)` to update
section options. The `rows` array property of `options` stores the cards of
sites to display. Each card object may have the following properties:

```js
{
  type, // One of the types in Card/types.js, e.g. "Trending"
  title, // Title string
  description, // Description string
  image, // Image url
  url // Site url
}
```
