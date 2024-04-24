# Other types of UI Widgets

In addition to the [new reusable UI widgets](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html) there are existing elements that serve a similar role.
These older elements are broken down into two groups: Mozilla Custom Elements and User Agent (UA) Widgets.
Additionally, we also have domain-specific widgets that are similar to the reusable widgets but are created to serve a specific need and may or may not adhere to the Design Systems specifications.

## Older custom elements in `toolkit/widgets`

There are existing UI widgets in `toolkit/content/widgets/` that belong to one of two groups: Mozilla Custom Elements or User Agent (UA) Widgets.
These [existing custom elements](https://searchfox.org/mozilla-central/rev/cde3d4a8d228491e8b7f1bd94c63bbe039850696/toolkit/content/customElements.js#792-809,847-866) are loaded into all privileged main process documents automatically.
You can determine if a custom element belongs to the existing UI widgets category by either [viewing the array](https://searchfox.org/mozilla-central/rev/cde3d4a8d228491e8b7f1bd94c63bbe039850696/toolkit/content/customElements.js#792-809,847-866) or by viewing the [files in toolkit/content/widgets](https://searchfox.org/mozilla-central/source/toolkit/content/widgets).
Additionally, these older custom elements are a mix of XUL and HTML elements.


### Mozilla Custom Elements

Unlike newer reusable UI widgets, the older Mozilla Custom Elements do not have a dedicated directory.
For example `arrowscrollbox.js` is an older single file custom element versus `moz-button-group/moz-button-group.mjs` which exemplifies the structure followed by newer custom elements.

### User Agent (UA) widgets

User agent (UA) widgets are like custom elements but run in per-origin UA widget scope instead of the chrome or content scope.
There are a much smaller number of these widgets compared to the Mozilla Custom Elements:
- [datetimebox.js](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/datetimebox.js)
- [marquee.js](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/marquee.js)
- [textrecognition.js](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/textrecognition.js)
- [videocontrols.js](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/videocontrols.js)

Please refer to the existing [UA widgets documentation](https://firefox-source-docs.mozilla.org/toolkit/content/toolkit_widgets/ua_widget.html) for more details.

### How to use existing Mozilla Custom Elements

The existing Mozilla Custom Elements are automatically imported into all chrome privileged documents.
These existing elements do not need to be imported individually via `<script>` tag or by using `window.ensureCustomElements()` when in a privileged main process document.
As long as you are working in a chrome privileged document, you will have access to the existing Mozilla Custom Elements.
You can dynamically create one of the existing custom elements by using `document.createDocument("customElement)` or `document.createXULElement("customElement")` in the relevant JS file, or by using the custom element tag in the relevant XHTML document.
For example, `document.createXULElement("checkbox")` creates an instance of [widgets/checkbox.js](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/checkbox.js) while using `<checkbox>` declares an instance in the XUL document.
You can see the checkbox widget in use in about:preferences.

### How to use existing UA Widgets

Please refer to the [existing UA widgets documentation](https://firefox-source-docs.mozilla.org/toolkit/content/toolkit_widgets/ua_widget.html) for more details on how to use these widgets.

### How these widgets behave in chrome versus in-content

The UA widgets are intended for web content, they may or may not work in-content or in the chrome.
However since these widgets run in a specific UA widget scope, their DOM cannot be accessed while in a content process.

The existing Mozilla Custom Elements can only be used in privileged content processes or in the chrome.
Since these elements cannot be used in regular web content, there is no difference in how they behave in the chrome versus in-content.

## Domain-specific Widgets

Not all reusable components should be used throughout the entirety of Firefox.
While we do have global UI patterns and components such as a button and modals, there are also specific features within the browser that include their own unique reusable patterns.
These patterns and components are known as "domain-specific widgets".
For example, the [login-timeline widget](https://searchfox.org/mozilla-central/source/browser/components/aboutlogins/content/components/login-timeline.mjs) is a domain-specific widget.

### Adding new domain-specific widgets

While we do have the `./mach addwidget` command, as noted in the [adding new design system components document](#adding-new-design-system-components), this command does not currently support the domain-specific widget case.
[See Bug 1828181 for more details on supporting this case](https://bugzilla.mozilla.org/show_bug.cgi?id=1828181).
Instead, you will need to use `./mach addstory your-widget "your-project-or-team-name"` to create a new story in `browser/components/storybook/stories`.
You can also use `./mach addstory your-widget "your-project-or-team-name" --path <your-widget-path>/<widget-source>` so that your widget's source is imported correctly.

If you specified a path and have Storybook running, a blank story entry for your widget will appear under the Domain-Specific UI Widgets/your-project section.
Otherwise, if you have not added a path, Storybook will throw an error saying that it "Can't resolve 'None'" in the newly added story.
This is expected and serves as a reminder that you need to update the newly created story with the widget source import.

Since Storybook is unaware of how the actual code is added or built, we won't go into detail about adding your new widget to the codebase.
It's recommended to have a `.html` test for your new widget since this lets you unit test the component directly rather than using integration tests with the domain.
To see what kind of files you may need for your widget, please refer back to [the output of the `./mach addwidget` command](#adding-new-design-system-components).
Just like with the UI widgets, [the `browser_all_files_referenced.js` will fail unless you use your component immediately outside of Storybook.](#known-browser_all_files_referencedjs-issue)

### Using new domain-specific widgets

This is effectively the same as [using new design system components](#using-new-design-system-components).
You will need to import your widget into the relevant `html`/`xhtml` files via a `script` tag with `type="module"`:

```html
<script type="module" src="chrome://browser/content/<domain-directory>/<your-widget>.mjs"></script>
```

Or use `window.ensureCustomElements("<your-widget>")` as previously stated in [the using new design system components section.](#using-new-design-system-components)
