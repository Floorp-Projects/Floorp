# Typography
## Scale
[In-content pages and the browser chrome](https://acorn.firefox.com/latest/resources/browser-anatomy/desktop-ZaxCgqkt) follow different type scales due to the chrome relying on operating systems' font sizing, while in-content pages follow the type scale set by the design system.

We set `font: message-box` at the root of `common-shared.css` and `global.css` stylesheets so that both in-content and the chrome can have access to operating system font families.

### In-content
<table class="sb-preview-design-tokens">
  <thead>
    <tr>
      <th>Name</th>
      <th>HTML class/tag or CSS token</th>
      <th>Preview</th>
      <th>Font size</th>
      <th>Font weight</th>
      <th>Line height</th>
    </tr>
  </thead>
  <tbody>
  <tr>
    <th>Heading XLarge <i>(used for error pages)</i></th>
      <td><code>--font-size-xxlarge</code></td>
      <td>
        ```html story
          <h1 class="text-truncated-ellipsis sb-preview-font-size-xxlarge">The quick brown fox jumps over the lazy dog</h1>
        ```
      </td>
      <td>
        <code>2.2rem</code> (<code>33px</code>)
      </td>
      <td>
        <code>300</code>
      </td>
      <td>
        <code>1.3</code>
      </td>
    </tr>
    <tr>
      <th>Heading Large</th>
      <td><code>h1,<br/>.heading-large</code></td>
      <td>
        ```html story
          <h1 class="text-truncated-ellipsis">The quick brown fox jumps over the lazy dog</h1>
        ```
      </td>
      <td>
        <code>1.467rem</code> (<code>22px</code>)
      </td>
      <td>
        <code>300</code>
      </td>
      <td>
        <code>1.3</code>
      </td>
    </tr>
    <tr>
      <th>Heading Medium</th>
      <td><code>h2,<br/>.heading-medium</code></td>
      <td >
        ```html story
          <h2 class="text-truncated-ellipsis">The quick brown fox jumps over the lazy dog</h2>
        ```
      </td>
      <td>
        <code>1.133rem</code> (<code>17px</code>)
      </td>
      <td>
        <code>600</code>
      </td>
      <td>
        <code>1.3</code>
      </td>
    </tr>
    <tr>
      <th>Root (body)</th>
      <td><code>--font-size-root</code> set at the <code>:root</code> of <code>common-shared.css</code></td>
      <td>
        ```html story
          <p class="text-truncated-ellipsis">The quick brown fox jumps over the lazy dog</p>
        ```
      </td>
      <td>
        <code>15px</code> (<code>1rem</code>)
      </td>
      <td>
        <code>normal</code>
      </td>
      <td>
        <code>1.5</code>
      </td>
    </tr>
    <tr>
      <th>Body Small</th>
      <td><code>--font-size-small</code></td>
      <td>
        ```html story
          <p class="text-truncated-ellipsis sb-preview-font-size-small">The quick brown fox jumps over the lazy dog</p>
        ```
      </td>
      <td>
        <code>0.867rem</code> (<code>13px</code>)
      </td>
      <td>
        <code>normal</code>
      </td>
      <td>
        <code>1.5</code>
      </td>
    </tr>
  </tbody>
</table>

### Chrome

The chrome solely relies on `font` declarations (it also relies on `font: menu` for panels) so that it can inherit the operating system font family **and** sizing in order for it to feel like it is part of the user's operating system. Keep in mind that font sizes and families vary between macOS, Windows, and Linux. Moreover, you will only see a difference between `font: message-box` and `font: menu` font sizes on macOS.

Note that there currently isn't a hierarchy of multiple headings on the chrome since every panel and modal that opens from it relies only on an `h1` for its title; so today, we just bold the existing fonts in order to create headings.

<table class="sb-preview-design-tokens">
  <thead>
    <tr>
      <th>Name</th>
      <th>Class</th>
      <th>Preview</th>
      <th>Font keyword</th>
      <th>Font weight</th>
      <th>Line height</th>
    </tr>
  </thead>
  <tbody>
     <tr>
      <th>Menu Heading</th>
      <td><code>h1,<br/>.heading-large</code></td>
      <td class="sb-preview-chrome-typescale sb-preview-chrome-menu">
        ```html story
          <h1 class="text-truncated-ellipsis">The quick brown fox jumps over the lazy dog</h1>
        ```
      </td>
      <td>
        <code>menu</code>
      </td>
      <td>
        <code>600</code>
      </td>
      <td>
        <code>normal</code>
      </td>
    </tr>
    <tr>
      <th>Menu</th>
      <td>Applied directly to panel classes in <code>panel.css</code> and <code>panelUI-shared.css</code></td>
      <td class="sb-preview-chrome-typescale sb-preview-chrome-menu">
         ```html story
          <p class="text-truncated-ellipsis">The quick brown fox jumps over the lazy dog</p>
        ```
      </td>
      <td>
        <code>menu</code>
      </td>
      <td>
        <code>normal</code>
      </td>
      <td>
        <code>normal</code>
      </td>
    </tr>
    <tr>
      <th>Heading</th>
      <td><code>h1,<br/>.heading-large</code></td>
      <td class="sb-preview-chrome-typescale">
        ```html story
          <h1 class="text-truncated-ellipsis">The quick brown fox jumps over the lazy dog</h1>
        ```
      </td>
      <td>
        <code>message-box</code>
      </td>
      <td>
        <code>600</code>
      </td>
      <td>
        <code>normal</code>
      </td>
    </tr>
    <tr>
      <th>Root (body)</th>
      <td><code>message-box</code> set at the <code>:root</code> of <code>global.css</code></td>
      <td class="sb-preview-chrome-typescale">
         ```html story
          <p class="text-truncated-ellipsis">The quick brown fox jumps over the lazy dog</p>
        ```
      </td>
      <td>
        <code>message-box</code>
      </td>
      <td>
        <code>normal</code>
      </td>
      <td>
        <code>normal</code>
      </td>
    </tr>
  </tbody>
</table>

## Design tokens
Type setting relies on typography design tokens for for font size, font weight, and line height.

#### Font size
<table class="sb-preview-design-tokens">
  <thead>
    <tr>
      <th>Base token</th>
      <th>In-content value</th>
      <th>Chrome value</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th>
        <code>--font-size-xlarge</code>
      </th>
      <td>
        <code>1.467rem</code>
      </td>
      <td>
        <code>unset</code>
      </td>
    </tr>
    <tr>
      <th>
        <code>--font-size-large</code>
      </th>
      <td>
        <code>1.133rem</code>
      </td>
      <td>
        <code>unset</code>
      </td>
    </tr>
    <tr>
      <th>
        <code>--font-size-root</code>
      </th>
      <td>
        <code>15px</code>
      </td>
      <td>
        <code>unset</code>
      </td>
    </tr>
    <tr>
      <th>
        <code>--font-size-small</code>
      </th>
      <td>
        <code>0.867rem</code>
      </td>
      <td>
        <code>unset</code>
      </td>
    </tr>
  </tbody>
</table>


#### Font weight
<table class="sb-preview-design-tokens">
  <thead>
    <tr>
      <th>Base token</th>
      <th>In-content value</th>
      <th>Chrome value</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th>
        <code>--font-weight-light</code>
      </th>
      <td>
        <code>300</code>
      </td>
      <td>
        <code>unset</code>
      </td>
    </tr>
    <tr>
      <th>
        <code>--font-weight-default</code>
      </th>
      <td>
        <code>400</code>
      </td>
      <td>
        <code>normal</code>
      </td>
    </tr>
    <tr>
      <th>
        <code>--font-weight-bold</code>
      </th>
      <td>
        <code>600</code>
      </td>
      <td>
        <code>600</code>
      </td>
    </tr>
  </tbody>
</table>

#### Line height
<table class="sb-preview-design-tokens">
  <thead>
    <tr>
      <th>Base token</th>
      <th>In-content value</th>
      <th>Chrome value</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th>
        <code>--line-height-small</code>
      </th>
      <td>
        <code>1.3</code>
      </td>
      <td>
        <code>unset</code>
      </td>
    </tr>
    <tr>
      <th>
        <code>--line-height-default</code>
      </th>
      <td>
        <code>1.5</code>
      </td>
      <td>
        <code>normal</code>
      </td>
    </tr>
  </tbody>
</table>

## Helpers
### text-and-typography.css

The text and typography stylesheet found in `toolkit/themes/shared/design-system/text-and-typography.css` contains type setting declarations, and text and typography helper classes:

-  It applies the design system's type scale by default, therefore it styles the `root` and headings automatically.
-  It comes with helper classes for contexts where designers may visually prefer an `h1` to start at the "medium" heading size instead of "large" (e.g. Shopping sidebar). It also contains text related helpers for truncating and deemphasizing text.

You should rely on typography helper classes and the defaults set by the design system.

This file is imported into `common-shared.css` and `global-shared.css` so that both in-content pages and the chrome receive their respective typography scale treatments, and have access to helper classes.

#### Heading
##### Large (h1)
###### In-content
```html story
  <h1>General</h1>
```

###### Chrome
```html story
  <h1 class="sb-preview-chrome-typescale">Close window and quit Firefox?</h1>
```

###### Chrome menus
```html story
  <h1 class="sb-preview-chrome-typescale sb-preview-chrome-menu">Edit bookmark</h1>
```

```css story
h1,
.heading-large {
  font-weight: var(--font-weight-light, var(--font-weight-bold));
  font-size: var(--font-size-xlarge);
  line-height: var(--line-height-small)
}
```

##### Medium (h2)
*Reminder: There's no hierarchy of headings on the chrome. So here's just in-content's preview:*

```html story
  <h2>Startup</h2>
```

```css story
h2,
.heading-medium {
  font-weight: var(--font-weight-bold);
  font-size: var(--font-size-large);
  line-height: var(--line-height-small);
}
```

#### Text
##### De-emphasized

```html story
  <span class="text-deemphasized">Get your passwords on your other devices.</span>
```

```css story
.text-deemphasized {
  font-size: var(--font-size-small);
  color: var(--text-color-deemphasized);
}
```

##### Truncated ellipsis

```html story
  <div class="text-truncated-ellipsis">A really long piece of text a really long piece of text a really long piece of text a really long piece of text a really long piece of text a really long piece of text.</div>
```

```css story
.text-truncated-ellipsis {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
```

`.text-truncated-ellipsis` can be applied to `display: block` or `display: inline-block` elements.

For `display: flex` or `display: grid` elements, you'll need to wrap its contents with an element with the `.text-truncated-ellipsis` class instead.

Example:

```html
<div class="my-flex-element">
  <span class="text-truncated-ellipsis">A really long string of text that needs truncation.</span>
</div>
```
