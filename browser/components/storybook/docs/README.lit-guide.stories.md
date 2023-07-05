# Lit

## Background

[Lit](https://lit.dev) is a small library for creating web components that is maintained by Google. It aims to improve the experience of authoring web components by eliminating boilerplate and providing more declarative syntax and re-rendering optimizations, and should feel familiar to developers who have experience working with popular component-based front end frameworks.

Mozilla developers began experimenting with using Lit to build a handful of new web components in 2021. The developer experience and productivity benefits were noticeable enough that the team tasked with building out a library of new [reusable widgets](./README.reusable-widgets.stories.md) vendored Lit to make it available in `mozilla-central` in late 2022. Lit can now be used for creating new web components anywhere in the codebase.

## Using Lit

Lit has comprehensive documentation on their website that should be consulted alongside this document when building new Lit-based custom elements: [https://lit.dev/docs/](https://lit.dev/docs/)

While Lit was initially introduced to assist with the work of the Reusable Components team it can also be used for creating both reusable and domain-specific UI widgets throughout `mozilla-central`. Some examples of custom elements that have been created using Lit so far include [moz-toggle](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-toggle/moz-toggle.mjs), [moz-button-group](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-button-group/moz-button-group.mjs), and the Credential Management team's [login-timeline](https://searchfox.org/mozilla-central/source/browser/components/aboutlogins/content/components/login-timeline.mjs) component.

### When to use Lit

Lit may be a particularly good choice if you're building a highly reactive element that needs to respond efficiently to state changes. Lit's declarative [templates](https://lit.dev/docs/templates/overview/) and [reactive properties](https://lit.dev/docs/components/properties/) can take care of a lot of the work of figuring out which parts of the UI should update in response to specific changes.

Because Lit components are ultimately just web components, you may also want to use it just because of some of the syntax it provides, like allowing you to write your template code next to your JavaScript code, providing for binding event listeners and properties in your templates, and automatically creating an open `shadowRoot`.

### When not to use Lit

Lit cannot be used in cases where you want to [extend a built-in element](https://developer.mozilla.org/en-US/docs/Web/API/Web_components/Using_custom_elements#customized_built-in_elements). Lit can only be used for creating [autonomous custom elements](https://developer.mozilla.org/en-US/docs/Web/API/Web_components/Using_custom_elements#autonomous_custom_elements), i.e. elements that extend `HTMLElement`.

## Writing components with Lit

All of the standard features of the Lit library - with the exception of decorators - are available for use in `mozilla-central`, but there are some special considerations and specific files you should be aware of when using Lit for Firefox code.

### Using external stylesheets

Using external stylesheets is the preferred way to style your Lit-based components in `mozilla-central`, despite the fact that the the Lit documentation [explicitly recommends against](https://lit.dev/docs/components/styles/#external-stylesheet) this approach. The caveats they list are not particularly relevant to our use cases, and we have implemented platform level workarounds to ensure external styles will not cause a flash-of-unstyled-content. Using external stylesheets makes it so that CSS changes can be detected by our automated linting and review tools, and helps provide greater visibility to Mozilla's `desktop-theme-reviewers` group.

### The `lit.all.mjs` vendor file

A somewhat customized, vendored version of Lit is available at [toolkit/content/widgets/vendor/lit.all.mjs](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/vendor/lit.all.mjs). The version of Lit in `mozilla-central` has a number of patches applied to disable minification, source maps, and certain warning messages, as well as patches to replace usage of `innerHTML` with `DOMParser` and to slightly modify the behavior of the `styleMap` directive. More specifics on these patches, as well as information on how to update `lit.all.mjs`, can be found [here](https://searchfox.org/mozilla-central/source/toolkit/content/vendor/lit).

Because our vendored version of Lit bundles the contents of a few different Lit source files into a single file, imports that would normally come from different files are pulled directly from `lit.all.mjs`. For example, imports that look like this when using the Lit npm package:

```js
// Standard npm package.
import { LitElement } from "lit";
import { classMap } from "lit/directives/class-map.js";
import { ifDefined } from "lit/directives/if-defined.js";
```

Would look like this in `mozilla-central`:

```js
// All imports come from a single file (relative path also works).
import { LitElement, classMap, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
```

### `MozLitElement` and `lit-utils.mjs`

[MozLitElement](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/lit-utils.mjs#84) is an extension of the `LitElement` class that has added functionality to make it more tailored to Mozilla developers' needs. In almost all cases `MozLitElement` should be used as the base class for your new Lit-based custom elements in place of `LitElement`.

It can be imported from `lit-utils.js` and used as follows:

```js
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

class MyCustomElement extends MozLitElement {
    ...
}
```

`MozLitElement` differs from `LitElement` in a few important ways:

#### 1. It provides automatic Fluent support for the shadow DOM

When working with Fluent in the shadow DOM an element's `shadowRoot` must be connected before Fluent can be used. `MozLitElement` handles this by extending `LitElement`'s `connectedCallback` to [call](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/lit-utils.mjs#84) `document.l10n.connectRoot` if needed. `MozLitElement` also automatically calls `document.l10n.translateFragment` on the renderRoot anytime an element updates. The net result of these modifications is that you can use Fluent in your Lit based components just like you would in any other markup in `mozilla-central`.

#### 2. It implements support for Lit's `@query` and `@queryAll` decorators

The Lit library includes `@query` and `@queryAll` [decorators](https://lit.dev/docs/components/shadow-dom/#@query-@queryall-and-@queryasync-decorators) that provide an easy way of finding elements within the internal component DOM. These do not work in `mozilla-central` as we do not have support for JavaScript decorators. Instead, `MozLitElement` provides equivalent [DOM querying functionality](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/lit-utils.mjs#87-99) via defining a static `queries` property on the subclass. For example the following Lit code that queries the component's DOM for certain selectors and assigns the results to different class properties:

```ts
import { LitElement, html } from "lit";
import { query } from "lit/decorators/query.js";

class MyCustomElement extends LitElement {
    @query("#title");
    _title;

    @queryAll("p");
    _paragraphs;

    render() {
        return html`
            <p id="title">The title</p>
            <p>Some other paragraph.</p>
        `;
    }
}
```

Is equivalent to this in `mozilla-central`:

```js
import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

class MyCustomElement extends MozLitElement {
    static queries = {
        _title: "#title", // equivalent to @query
        _paragraphs: { all: "p" }, // equivalent to @queryAll
    };

    render() {
        return html`
            <p id="title">The title</p>
            <p>Some other paragraph.</p>
        `;
    }
}
```

#### 3. It adds a `dispatchOnUpdateComplete` method

The `dispatchOnUpdateComplete` method provides an easy way to communicate to test code or other element consumers that a reactive property change has taken effect. It leverages Lit's [updateComplete](https://lit.dev/docs/components/lifecycle/#updatecomplete) promise to emit an event after all updates have been applied and the component's DOM is ready to be queried. It has the potential to be particularly useful when you need to query the DOM in test code, for example:

```js
// my-custom-element.mjs
class MyCustomElement extends MozLitElement {
    static properties = {
        clicked: { type: Boolean },
    };

    async handleClick() {
        if (!this.clicked) {
            this.clicked = true;
        }
        this.dispatchOnUpdateComplete(new CustomEvent("button-clicked"));
    }

    render() {
        return html`
            <p>The button was ${this.clicked ? "clicked" : "not clicked"}</p>
            <button @click=${this.handleClick}>Click me!</button>
        `;
    }
}
```

```js
// test_my_custom_element.mjs
add_task(async function testButtonClicked() {
    let { button, message } = this.convenientHelperToGetElements();
    is(message.textContent.trim(), "The button was not clicked");

    let clicked = BrowserTestUtils.waitForEvent(button, "button-clicked");
    synthesizeMouseAtCenter(button, {});
    await clicked;

    is(message.textContent.trim(), "The button was clicked");
});
```
