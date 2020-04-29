# Application

## About the Application panel

The Application panel is a Firefox Developer Tools panel meant to allow the inspection and debugging of features usually present in Progressive Web Apps, such as service workers or the Web App Manifest.

### How to enable the Application panel

At the moment, the panel is shown by default on Nightly and Dev Edition builds only, behind a configuration flag. In order to enable it, type `about:config` in the address bar, look for the `devtools.application.enabled` flag and set it to true. It will then appear as a tab with the rest of the panels within the Developer Tools toolbox.

## Technical overview

The Application panel is a React + Redux web app.

### Source directory structure

The application panel lives in the `devtools/client/application` directory. Inside this root directory, the most relevant subdirectories and files are:

```
.
├── application.css -> root CSS stylesheet
├── initializer.js -> panel initialization
├── src
│   ├── actions -> Redux actions
│   ├── components -> React components
│   ├── modules -> Business logic and helpers
│   └── reducers -> Redux reducers and states
└── test
    ├── browser -> mochitests (e2e/integration)
    ├── node -> Jest unit tests
    └── xpcshell -> xpcshell unit tests
```

### Panel registration

The panel is defined in `devtools/client/application/panel.js` – which in turn calls the `.bootstrap()` method defined in `devtools/client/application/initializer.js`.

The panel is registered along the rest of the Developer Tools panels, in `devtools/client/definitions.js`.

### Localization

The panel uses the [fluent-react](https://github.com/projectfluent/fluent.js/wiki/React-Bindings) library. The localization file is located at `devtools/client/locales/en-US/application.ftl` and it follows [Fluent syntax](https://projectfluent.org/fluent/guide/).

You should read the [Fluent for Firefox developers](https://firefox-source-docs.mozilla.org/intl/l10n/l10n/fluent_tutorial.html) and [Guidelines for Fluent Reviewers](https://firefox-source-docs.mozilla.org/intl/l10n/l10n/fluent_review.html) guides.

#### Adding a new string

If you need to localize a text, add a new message ID to the localization file (`devtoos/client/en-US/application.ftl`). Then you can use this ID as a prop for Fluent's `<Localized>` component or `getString()` function.

When using ID's from the React code, always write the full ID instead of building it with concatenation, interpolation, etc. (this makes localing ID's easier afterwards). For instance, let's say you have two ID's, `error-message` and `warning-message`, and you need to use one or the other depending on a condition.

✅ **Do:**

```js
const localizationIds = {
  error: "error-message",
  warning: "warning-message",
};

const id = localizationIds[messageLevel];
```

❌ **Don't:**

```js
const id = `${messageLevel}-message`;
```

#### Updating an existing string

If you need to modify an existing string, you need to create a new ID and don't remove the previous one. For instance, if we had this string:

```
some-localized-string = This will be modified later
```

And we need to update the content, we would create a new ID and remove the previous one intact:

```diff
- some-localized-string = This will be modified later
+ some-localized-string2 = This is the updated string
```

Within the React component code, you would use the newly created ID.

```js
const localizedText = l10n.getString("some-localized-string2");
```

### React components

Components are located in the `devtools/client/application/src/components` directory.

Each component has a single `.js` file, plus an optional `.css` file, so each component is responsible of handling their own styles.

- The `App` component is the root component of the Application panel.
- In `components/service-workers` are the components that render all of the UI related to inspect and debug service workers.
- In `components/manifest` are the components that render everything related to inspecting a Web App Manifest.
- In the `components/routing` directory there are components related to switching from one section of the panel to the other, like a sidebar or a page switcher container.
- Inside the `components/ui` directory there are generic UI components that can be reused from other parts of the panel.

### Redux

The Application panel uses Redux to handle app state and communication between components.

#### Substates

The Redux state is set up in `devtools/client/application/src/create-store.js`. The state contains the following substates:

- `workers`: contains data related to inspecting and debugging service workers.
- `manifest`: contains state related to inspecting the Web App Manifest.
- `page`: contains general data related to the web page that is being inspected.
- `ui`: contains data related to the UI that don't really fit any other state.

#### Actions

Synchronous actions are regular Redux actions. Their type is defined in the general constants file (`devtools/client/application/src/constants.js`).

**Asynchronous** actions are achieved thanks to the **thunk middleware**. They follow a three-action pattern `*_START`, `*_SUCCESS`, `*_FAILURE`. For instance:

```js
function fooAction() {
  return async (dispatch, getState) => {
    dispatch({ type: FOO_START });

    try {
      const result = await foo();
      dispatch({ type: FOO_SUCCESS });
    }
    catch (err) {
      console.error(err);
      dispatch({ type: FOO_FAILURE });
    }
  }
}
```

#### Middleware

We are using the following middlewares with Redux:

- [redux-thunk](https://github.com/reduxjs/redux-thunk): This is a shared middleware used by other DevTools modules that allows to dispatch asynchronous actions.

### Constants

We are sharing some constants / enum-like constants across the source. They are located in `devtools/client/application/src/constants.js`. For values that are not shared, they should stay within their relevant scope/file.

## Tests

You should read DevTools' general [guidelines for automated tests](https://docs.firefox-dev.tools/tests/).

### Mochitests (e2e / integration)

End to end and integration tests are made with **Mochitests**. These tests open a browser and will load an HTML and open the toolbox with the Application panel.

These tests are located in `devtools/client/application/test/browser`.

Besides the usual DevTools test helpers, the Application panel adds some other helper functions in `devtools/client/application/test/browser/head.js`

### Unit tests with xpcshell

We are using xpcshell unit tests for the **Redux reducers** in `devtools/client/application/test/xpcshell`.

Other unit tests that don't need Enzyme or other npm modules should be added here too.

### Unit tests with Jest

We are using Jest with Enzyme to test **React components** and **Redux actions** in `devtools/client/application/test/node`. Some considerations:

- There are some **stubs/mocks** for some libraries and modules. These are placed in `devtools/client/application/test/node/fixtures`.
- The **localization system is mocked**. For `<Localized>` components, the Enzyme snapshots will reflect the localization ID. Ex: `<Localized id="some-localization">`. The actual localized string will not be rendered with `.shallow()`. Calls to `getstring()` will return the string ID that we pass as an argument.
- The **redux store is mocked** as well. To create a fake store to test a component that uses `connect()` to fetch data from the store, use the helper `setupStore()` included in `devtools/client/application/test/node/helpers.js`.

#### Snapshots

Most Jest tests will involve a snapshot matching expectation.

**Snapshots should be produced with [`shallow()`](https://airbnb.io/enzyme/docs/api/shallow.html)** and _not_ `mount()`, to ensure that we are only testing the current component.

**Components that are wrapped** with `connect()` or other wrapper components, need their snapshot to be rendered with [`dive()`](https://airbnb.io/enzyme/docs/api/ShallowWrapper/dive.html) (one call to `dive()` per wrapper component).

## CSS

We are using a BEM-like style for the following reasons:

- To avoid collision between the styles of components.
- To have a consistent methodology on how to use CSS.
- To ensure we have rules with low specificity and thus styles are easier to override and maintain.

### Stylesheet organization

#### File structure

**Each component has its own CSS stylesheet**, in the same directory and with the same name as the JavaScript file, only with a `.css` extension. So `Foo.js` would have a `Foo.css` stylesheet.

These stylesheets are imported from `devtools/client/application/application.css` in alphabetical order –BEM-like naming and low specificity, when done right, should ensure that styles work regardless of the order the stylesheets are included.

#### Base styles

Another important stylesheet is `devtools/client/application/base.css`, which is the one that is imported first from `application.css`, regardless the alphabetical order. In this stylesheet we do:

- Variables definitions
- Utility classes
- Some styles that are applied to the whole panel, like some resets or default values.

#### External stylesheets imported in the panel

Besides the usual user-agent styles, all DevTools panels automatically import the stylesheets located at `devtools/client/themes/`. The most relevant for the Application panel are:

- `devtools/client/themes/variables.css`: variables are defined here
- `devtools/client/themes/common.css`: rules that are applied to every panel

In the Application panel, we are not using some of the classes and rules defined in `common.css`, since they don't follow the BEM convention and some of them have high specificity. However, there are rules defined at the element level and this will get applied to the markup we use.

From `variables.css`, at the moment we mainly use color names. See the section below for details. Other variables available there that contain values for common elements (like heights, widths, etc.) should also be used when we implement those components.

### Colors and themes

All DevTools panels should support both **light and dark themes**. The active theme can be changed by the user at DevTools Settings.

The way this is handled is that a `.theme-light` or `.theme-dark` class is automatically added to the root `<html>` element of each panel. Then, in `variables.css`, there are some CSS variables with different values depending on which of those classes the root element has.

- We should use the colors defined in `variables.css`.
- If the other panels are using a particular color for an element we also have, we should re-use the same color.
- If needed, we can create a CSS variable in `base.css` that takes the value of another color variable defined in `variables.css`
- If the desired color is not available in `variables.css`, we can create a new one, but we have to define values for both light and dark themes.

#### Writing CSS selectors

As mentioned earlier, we are using a system to create rules similar to [BEM](http://getbem.com/naming/), although not strictly identical.

#### Low specificity rules

Rules should have **the lowest specifity** that is possible. In practice, this means that we should try to use a **single class selector** for writing the rule. Ex:

```css
.manifest-item {
  /* ... */
}
```

Note that **pseudoclasses are pseudoselectors are OK** to use.

#### Hierarchy

When an element is semantically the child of another element, we should also use a single class selector. In BEM, this is achieved by separating potential class names with a double underscore `__`, so we have a single classname.

✅ **Do:**

```css
.manifest-item { /* ... */ }
.manifest-item__value { /* ... */ }
```

❌ **Don't:**

```css
.manifest-item { /* ... */ }
.manifest-item .value { /* ... */ }
```

We should not go deeper than one level of `__`. We don't have to replicate the whole DOM structure:

- If the style rule should be independent of the parent, it does not need to use an `__`.
- If the style rule does not make sense on its own, and it will be scoped to a particular section of the DOM tree, don't replicate the whole DOM structure and use the top-level ancestor class name instead.

For instance, if we have the following HTML markup and we have to choose a class name for the `<dt>` element:

```html
<li class="worker">
  <dl class="worker__data">
    <dt class="..."></dt>
    <!-- ... -->
```

✅ **Do:**

```css
.worker__meta-name { /* ... */ }
```

❌ **Don't:**

```css
.worker__data__meta-name { /* ... */ }
```

#### Style variations

Sometimes we have a component that should change its style depending on its state, or we have some rules that allow us to customize further how something looks. For instance, we could have a generic rule for buttons, and then some extra rules for primary buttons, or for tiny ones.

In BEM, style variants are created with a double hyphen `--`. For example:

```css
.ui-button { /* styles for buttons */ }
.ui-button--micro { /* specific styles for tiny buttons */ }
```

Then, in the component, _both_ classes are to be used.

```html
<button class="ui-button ui-button--micro">
```

#### Base size

Photon has a base unit of `4px` for sizes, paddings, margins… This value is stored in the CSS variable `--base-unit`.

Our sizes and other dimensions should be defined based on this variable. Since we are using raw CSS and not a pre/post processor, we need to use `calc()` to achieve this.

✅ **Do:**

```css
.ui-button {
  height: calc(var(--base-unit) * 6);
  /* ... */
}
```

❌ **Don't:**

```css
.ui-button {
  height: 24px;
  /* ... */
}
```

---

## Contact

If you have questions about the code, features, planning, etc. the current active team is:

- Product management: Harald Kischner (`:digitarald`)
- Engineering management: Patrick Brosset (`:pbro`)
- Engineering: Belén Albeza (`:ladybenko`)
- Engineering: Ola Gasidlo (`:ola`)
