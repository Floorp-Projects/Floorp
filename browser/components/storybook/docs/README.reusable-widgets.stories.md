# Reusable UI widgets

## Background

Different Firefox surfaces make use of similar UI elements such as cards, menus,
toggles, and message bars. A group of designers and developers have started
working together to create standardized versions of these elements in the form
of new web components. The intention is for these components to encapsulate our
design system, ensure accessibility and usability across the application, and
reduce the maintenance burden associated with supporting multiple different
implementations of the same UI patterns.

Many of these components are being built using the [Lit
library](https://lit.dev/) to take advantage of its templating syntax and
re-rendering logic. All new components are being documented in Storybook in an
effort to create a catalog that engineers and designers can use to see which
components can be easily lifted off the shelf for use throughout Firefox.

## Adding new design system components

We have a `./mach addwidget` scaffold command to make it easier to create new
reusable components and hook them up to Storybook. Currently this command can
only be used to add a new Lit based web component to `toolkit/content/widgets`.
In the future we may expand it to support options for creating components
without using Lit and for adding components to different directories.

To create a new component, you run:

```sh
# Component names should be in kebab-case and contain at least 1 -.
./mach addwidget component-name
```

The scaffold command will generate the following files:

```sh
└── toolkit
    └── content
        ├── tests
        │   └── widgets
        │       └── test_component_name.html # chrome test
        └── widgets
            └── component-name # new folder for component code
                ├── component-name.css # component specific CSS
                ├── component-name.mjs # Lit based component
                └── component-name.stories.mjs # component stories
```

It will also make modifications to `toolkit/content/jar.mn` to add chrome://
URLs for the new files, and to `toolkit/content/tests/widgets/chrome.ini` to
enable running the newly added test.

After running the scaffold command you can start Storybook and you will see
placeholder content that has been generated for your component. You can then
start altering the generated files and see your changes reflected in Storybook.

Unfortunately for now the
[`browser_all_files_referenced.js`](https://searchfox.org/mozilla-central/source/browser/base/content/test/static/browser_all_files_referenced.js)
test will fail unless your new component is immediately used somewhere outside
of Storybook. We have plans to fix this issue, but for now you can get around it
by updating [this array](https://searchfox.org/mozilla-central/source/browser/base/content/test/static/browser_all_files_referenced.js#107) to include your new chrome filepath.

## Using new components

Once you've added a new component to `toolkit/content/widgets` and created
chrome:// URLs via `toolkit/content/jar.mn` you should be able to start using it
throughout Firefox. You can import the component into `html`/`xhtml` files via a
`script` tag with `type="module"`:

```html
<script type="module" src="chrome://global/content/elements/your-component-name.mjs"/>
```

### Common pitfalls

If you're trying to use a reusable component but nothing is appearing on the
page it may be due to one of the following issues:

- Omitting the `type="module"` in your `script` tag.
- Not specifying the `html:` namespace when using a custom HTML element in an
  `xhtml` file. For example the tag should look something like this:

  ```html
  <html:your-component-name></html:your-component-name>
  ```
- Adding a `script` tag to an `inc.xhtml` file. For example when using a new
  component in the privacy section of `about:preferences` the `script` tag needs
  to be added to `preferences.xhtml` rather than to `privacy.inc.xhtml`.
- Trying to extend a built-in HTML element in Lit. Because Webkit never
  implemented support for customized built-ins Lit doesn't support it either.
  That means if you want to do something like:

  ```js
  customElements.define("cool-button", CoolButton, { extends: "button" });
  ```

  you will need to make a vanilla custom element, you cannot use Lit. For an
  example of how this works in practice, see
  [`moz-support-link`](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-support-link/moz-support-link.mjs).
