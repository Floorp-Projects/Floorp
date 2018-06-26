# CSS

This page is for information about CSS used by DevTools. Wondering about the Dev Edition theme? See this page for more information about the [Developer Edition theme](https://wiki.mozilla.org/DevTools/Developer_Edition_Theme).

## Basics

The CSS code is in `devtools/client/themes`.

Here are some basic tips that can optimize reviews if you are changing CSS:

* Avoid `!important` but if you have to use it, make sure it's obvious why you're using it (maybe with a comment).
* Avoid magic numbers, prefer automatic sizing.
* Avoid platforms specific styles, put everything in the `shared` directory.
* Avoid preprocessor variables, use CSS variables instead.
* Avoid setting styles in JavaScript. It's generally better to set a class and then specify the styles in CSS
* `classList` is generally better than `className`. There's less chance of over-writing an existing class.

### Boilerplate

Make sure each file starts with the standard copyright header (see [License Boilerplate](https://www.mozilla.org/MPL/headers/)).

### Testing

CSS changes should generally be similar across platforms since they used a shared implementation, but there can still be differences worth checking out. Check major changes on Windows, OS X and Ubuntu.

## Formatting

We use 2-spaces indentation for the CSS.

In general the formatting looks like this:

```css
selector,
alternate-selector {
  property: value;
  other-property: other-value;
}
```
<!--TODO: add examples for long shorthand properties, and multi-valued properties (background, font-family, ...)-->
Also:

* Omit units on 0 values.
 * Example: Use `margin: 0;`, not `margin: 0px;`.
* Add a space after each comma, **except** within color functions.
 * Example: `linear-gradient(to bottom, black 1px, rgba(255,255,255,0.2) 1px)`.
* Always add a space before ` !important`.
* Assume `="true"` in attribute selectors.
 * Example: Use `option[checked]`, not `option[checked="true"]`.
* Use longhand versions of properties so it's clear what you're changing.
 * Example: Use `border-color: red`, not `border: red;`.

Naming standards for class names:

* `lower-case-with-dashes` is the most common.
* But `camelCase` is also used sometimes. Try to follow the style of existing or related code.

## Light and Dark theme support

DevTools supports 2 different themes: the dark theme and the light theme. In order to support them, there are 2 class names available (`theme-dark` and `theme-light`).

* Use [pre-defined CSS variables](https://developer.mozilla.org/en-US/docs/Tools/DevToolsColors) instead of hardcoding colors when possible.
* If you need to support themes and the pre-defined variables don't fit, define a variable with your custom colors at the beginning of the CSS file. This avoids selector duplication in the code.

Example:

```css
.theme-light {
  --some-variable-name: <color-for-light-theme>;
}
.theme-dark {
  --some-variable-name: <color-for-dark-theme>;
}
#myElement {
  background-color: var(--some-variable-name);
}
```

## HDPI support

It's recommended to use SVG since it keeps the CSS clean when supporting multiple resolutions. However, if only 1x and 2x PNG assets are available, you can use this `@media` query to target higher density displays (HDPI): `@media (min-resolution: 1.1dppx)`. <!--TODO an example would be good here-->

## Performance

* Read [Writing Efficient CSS](https://developer.mozilla.org/en-US/docs/Web/Guide/CSS/Writing_efficient_CSS).
* Use an iframe where possible so your rules are scoped to the smallest possible set of nodes.<!--TODO: is this still true? and also refine exactly when it is appropriate to use an iframe. Examples might help-->
* If your CSS is used in `browser.xul`, you need to take special care with performance:
 * Descendent selectors should be avoided.
 * If possible, find ways to use **only** id selectors, class selectors and selector groups.

## Localization

### Text Direction
* For margins, padding and borders, use `inline-start`/`inline-end` rather than `left`/`right`.
 * Example: Use `margin-inline-start: 3px;` not `margin-left: 3px`.
* For RTL-aware positioning (left/right), use `inset-inline-start/end`.
* When there is no special RTL-aware property (eg. `float: left|right`) available, use the pseudo `:-moz-locale-dir(ltr|rtl)` (for XUL files) or `:dir(ltr|rtl)` (for HTML files).
* Remember that while a tab content's scrollbar still shows on the right in RTL, an overflow scrollbar will show on the left.
* Write `padding: 0 3px 4px;` instead of `padding: 0 3px 4px 3px;`. This makes it more obvious that the padding is symmetrical (so RTL won't be an issue).

### RTL support for html modules

By default, new HTML modules support only left-to-right (LTR) and do not reuse the current direction of the browser.

To enable right-to-left (RTL) support in a module, set the `[dir]` attribute on the document element of the module:
* Example: `<html xmlns="http://www.w3.org/1999/xhtml" dir="">`.

The appropriate value for the `dir` attribute will then be set when the toolbox loads this module.

### Testing

The recommended workflow to test RTL on DevTools is to use the [Force RTL extension](https://addons.mozilla.org/en-US/firefox/addon/force-rtl/). After changing the direction using Force RTL, you should restart DevTools to make sure all modules apply the new direction. A future version of Force RTL will be able to update dynamically all DevTools documents.<!--TODO: update when the fate of this addon/webextension is known--> 

Going to `about:config` and setting `intl.uidirection.en` to rtl is not recommended, and will always require to re-open DevTools to have any impact.

## Toggles

Sometimes you have a style that you want to turn on and off. For example a tree twisty (a expand-collapse arrow), a tab background, etc.

The Mozilla way is to perform the toggle using an attribute rather than a class:

```css
.tree-node {
  background-image: url(right-arrow.svg);
}
.tree-node[open] {
  background-image: url(down-arrow.svg);
}
```

## Tips

* Use `:empty` to match a node that doesn't have children.
* Usually, if `margin` or `padding` has 4 values, something is wrong. If the left and right values are asymmetrical, you're supposed to use `-start` and `-end`. If the values are symmetrical, use only 3 values (see localization section).

