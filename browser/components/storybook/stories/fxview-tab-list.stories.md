# FxviewTabList

`fxview-tab-list` is a list of `fxview-tab-row` elements that display tab info such as:
* A link containing:
    * Favicon
    * Title
    * Domain
    * Time when tab was last accessed (can be formatted as `relative`, `date and time`, `date only`, and `time only`)
* Secondary action button

## When to use

* Use `fxview-tab-list` anywhere you want to display a list of tabs with the above info displayed.

## Code

The source for `fxview-tab-list` can be found under
[browser/components/firefoxview/fxview-tab-list](https://searchfox.org/mozilla-central/source/browser/components/firefoxview/fxview-tab-list.mjs).

`fxview-tab-list` can be imported into `.html`/`.xhtml` files:

```html
<script type="module" src="chrome://content/browser/firefoxview/fxview-tab-list.mjs"></script>
```

And used as follows:

With context menu:
```html
<fxview-tab-list
    class="with-context-menu"
    .dateTimeFormat=${"relative"}
    .hasPopup=${"menu"}
    .maxTabsLength=${this.maxTabsLength}
    .tabItems=${this.tabItems}
    @fxview-tab-list-secondary-action=${this.onSecondaryAction}
    @fxview-tab-list-primary-action=${this.onPrimaryAction}
>
    <panel-list slot="menu" @hide=${this.menuClosed}>
        <panel-item
        @click=${...}
        data-l10n-id="fxviewtabrow-delete"
        ></panel-item>
        <panel-item
        @click=${...}
        data-l10n-id="fxviewtabrow-forget-about-this-site"
        ></panel-item>
        ...
    </panel-list>
</fxview-tab-list>
```
With dismiss button:
```html
<fxview-tab-list
    class="with-dismiss-button"
    .dateTimeFormat=${"relative"}
    .maxTabsLength=${this.maxTabsLength}
    .tabItems=${this.tabItems}
    @fxview-tab-list-secondary-action=${this.onSecondaryAction}
    @fxview-tab-list-primary-action=${this.onPrimaryAction}
></fxview-tab-list>
```

### Properties

You'll need to pass along some of the following properties:
* `compactRows` (**Optional**): If `true`, displays shorter rows by omitting the URL and date/time. The default value is `false`.
* `dateTimeFormat` (**Optional**): A string to indicate the expected format/options for the date and/or time displayed on each tab item in the list. The default for this if not given is `"relative"`.
    * Options include:
        * `relative` (Ex: "Just now", "2m ago", etc.)
        * `date` (Ex: "4/1/23", "01/04/23", etc. - Will be formatted based on locale)
        * `time` (Ex: "4:00 PM", "16:00", etc - Will be formatted based on locale)
        * `dateTime` (Ex: "4/1/23 4:00PM", "01/04/23 16:00", etc. - Will be formatted based on locale)
* `hasPopup` (**Optional**): The optional aria-haspopup attribute for the secondary action, if required
* `maxTabsLength` (**Optional**): The max number of tabs you want to display in the tabs list. The default value will be `25` if no max value is given. You may use any negative number such as `-1` to indicate no max.
* `tabItems` (**Required**): An array of tab data such as History nodes, Bookmark nodes, Synced Tabs, etc.
    * The component is expecting to receive the following properties within each `tabItems` object (you may need to do some normalizing for this). If you just pass a title and an icon, it creates a header row that is not clickable.
        * `closedId` (**Optional**) - For a closed tab, this ID is used by SessionStore to retrieve the tab data for forgetting/re-opening the tab.
        * `icon` (**Required**) - The location string for the favicon. Will fallback to default favicon if none is found.
        * `primaryL10nId` (**Optional**) - The l10n id to be used for the primary action element. This fluent string should ONLY define a `.title` attribute to describe the link element in each row.
        * `primaryL10nArgs` (**Optional**) - The l10n args you can optionally pass for the primary action element
        * `secondaryL10nId` (**Optional**) -  The l10n id to be used for the secondary action button. This fluent string should ONLY define a `.title` attribute to describe the secondary button in each row.
        * `secondaryL10nArgs` (**Optional**) - The l10n args you can optionally pass for the secondary action button
        * `tabElement` (**Optional**) - The MozTabbrowserTab element for the tab item.
        * `sourceClosedId` (**Optional**) - The closedId of the closed window the tab is from if applicable.
        * `sourceWindowId` (**Optional**) - The SessionStore id of the window the tab is from if applicable.
        * `tabid` (**Optional**) - Optional property expected for Recently Closed tab data
        * `time` (**Optional**) - The time in milliseconds for expected last interaction with the tab (Ex: `lastUsed` for SyncedTabs tabs, `closedAt` for RecentlyClosed tabs, etc.)
        * `title` (**Required**) - The title for the tab
        * `url` (**Optional**) - The full URL for the tab
* `searchQuery` (**Optional**) - Highlights matches of the query string for titles of each row.


### Notes

* In order to keep this as generic as possible, the icon for the secondary action button will NOT have a default. You can supply a `class` attribute to an instance of `fxview-tab-list` in order to apply styles to things like the icon for the secondary action button. In the above example, I added a class `"with-context-menu"` to `fxview-tab-list`, so I can update the button's icon by using:
```css
    fxview-tab-list.with-context-menu::part(secondary-button) {
      background-image: url("chrome://global/skin/icons/more.svg");
    }
```
* You'll also need to define functions for the `fxview-tab-list-primary-action` and `fxview-tab-list-secondary-action` listeners in order to add functionality to the primary element and the secondary button.
    * For the both handler functions, you can reference the `fxview-tab-row` associated with the interaction by using `event.originalTarget`. You can also reference the original event by using `event.detail.originalEvent`.
