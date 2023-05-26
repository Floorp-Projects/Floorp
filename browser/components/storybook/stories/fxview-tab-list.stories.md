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
    .secondaryL10nId=${"fxviewtabrow-open-menu"}
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
    .secondaryL10nId=${"fxviewtabrow-dismiss-tab-button"}
    .tabItems=${this.tabItems}
    @fxview-tab-list-secondary-action=${this.onSecondaryAction}
    @fxview-tab-list-primary-action=${this.onPrimaryAction}
></fxview-tab-list>
```

### Notes

* You'll need to defines function for `this.onPrimaryAction` and `this.onSecondaryAction` in order to add functionality to the primary element and the secondary button
* You can also supply a `class` attribute to the instance on `fxview-tab-list` in order to apply styles to things like the icon for the secondary action button:
For example:
```css
    fxview-tab-list.with-context-menu::part(secondary-button) {
      background-image: url("chrome://global/skin/icons/more.svg");
    }
```

### Properties

You'll need to pass along some of the following properties:
* `dateTimeFormat`: A string to indicate the expected format/options for the date and/or time displayed on each tab item in the list. The default for this if not given is `"relative"`.
    * Options include:
        * `relative` (Ex: "Just now", "2m ago", etc.)
        * `date` (Ex: "4/1/23", "01/04/23", etc. - Will be formatted based on locale)
        * `time` (Ex: "4:00 PM", "16:00", etc - Will be formatted based on locale)
        * `dateTime` (Ex: "4/1/23 4:00PM", "01/04/23 16:00", etc. - Will be formatted based on locale)
* `hasPopup`: The optional aria-haspopup attribute for the secondary action, if required
* `maxTabsLength`: The max number of tabs you want to display in the tabs list. The default value will be `25` if no max value is given.
* `secondaryL10nId`: The l10n ID of the secondary action button
* `tabItems`: An array of tab data such as History nodes, Bookmark nodes, Synced Tabs, etc.
    * The component is expecting to receive the following properties within each `tabItems` object (you may need to do some normalizing for this):
        * `icon` - The location string for the favicon. Will fallback to default favicon if none is found.
        * `primaryL10nId` - The l10n id to be used for the primary action element
        * `primaryL10nArgs` - The l10n args you can optionally pass for the primary action element
        * `secondaryL10nId` - The l10n id to be used for the secondary action button
         * `secondaryL10nArgs` - The l10n args you can optionally pass for the secondary action button
        * `tabid` - Optional property expected for Recently Closed tab data
        * `time` - The time in milliseconds for expected last interaction with the tab (Ex: `lastUsed` for SyncedTabs tabs, `closedAt` for RecentlyClosed tabs, etc.)
        * `title` - The title for the tab
        * `url` - The full URL for the tab
