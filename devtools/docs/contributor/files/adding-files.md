# Adding New Files - Various DevTools Resource Types

This page lists the various DevTools resource types and how they can be created and loaded.

## JavaScript Modules

### Build Configuration

JavaScript modules are installed by our build system using `moz.build` files. If you add a new JavaScript module, you'll need to update (or add) one of these files to make the build system aware of your new module. See the example below.

A `moz.build` file must live in the same directory as the files to be installed. Don't list files from a subdirectory in a `moz.build` from a parent directory.

Following these steps ensures that `require()` and `resource://` paths map directly to locations in the source tree, instead of being totally arbitrary.

Example:

* File: `/devtools/server/actors/layout.js`
* In `/devtools/server/actors/moz.build`:

```
DevToolsModules(
  'layout.js'
)
```

### `require()`

Most DevTools JS code is in the form of CommonJS modules that are loaded with `require()`.

To `require()` a file, the module ID is exactly its source tree path.

Example:

* File: `/devtools/server/actors/layout.js`
* Usage (prefer lazy in most cases):
  * `loader.lazyRequireGetter(this, "layout", "devtools/server/actors/layout")`
  * `require("devtools/server/actors/layout")`

### `ChromeUtils.import()`

Some older DevTools JS modules use the Gecko JavaScript code module format with the file extension `.jsm`. We are trying to move away from this format, so it's unlikely you would add a new one, but you might need to import an existing one in your code.

These modules are loaded using `ChromeUtils.import()`. To `import()` a file, you provide a `resource://` URL, which is exactly the source tree path.

In more detail:

* `/devtools/client/<X>: resource://devtools/client/<X>`
* `/devtools/server/<X>: resource://devtools/server/<X>`
* `/devtools/shared/<X>: resource://devtools/shared/<X>`

Example:

* File: `/devtools/shared/loader/Loader.sys.mjs`
* Usage:
  * `const { loader } = ChromeUtils.importESModule("resource://devtools/shared/loader/Loader.sys.mjs")`

Example:

* File: `/toolkit/mozapps/extensions/AddonManager.jsm`
* Usage (prefer lazy in most cases):
  * `const lazy = {}; ChromeUtils.defineModuleGetter(lazy, "AddonManager", "resource://gre/modules/AddonManager.jsm")`
  * `const { AddonManager } = ChromeUtils.import("resource://gre/modules/AddonManager.jsm")`

## Chrome Content

Much of the DevTools front-end / UI is currently loaded using `chrome://` URLs, which allow those files to have privileged access to platform internals.

This is typically used to load XUL, HTML, and JS files in the UI.

Note: "Chrome" here means "browser chrome", as in the UI, and bears no relation to "Chrome" as in the browser. We'd like to move away from this on DevTools and be more like regular web sites, but most tools are using `chrome://` URLs for now.

### Packaging

If you add a new file that should be loaded via a `chrome://` URL, you need to update a manifest file at `/devtools/client/jar.mn` so that it's packaged correctly.

Please ensure that any new files are added so their entire source tree path is part of the URL. To do so, the `jar.mn` entry should look like:

```
content/<X> (<X>)
```

where `<X>` is the path to your file after removing the `/devtools/client/` prefix.

Example:

* File: `/devtools/client/webaudioeditor/models.js`
* Entry: `content/webaudioeditor/models.js (webaudioeditor/models.js)`

### Usage

Chrome content URLs almost match their source tree path, with one difference: the segment `client` is replaced by `content`. This is a requirement of the `chrome://` protocol handler.

Example:

* File: `/devtools/client/webaudioeditor/models.js`
Usage: `chrome://devtools/content/webaudioeditor/models.js`

For files within a single tool, consider relative URLs. They're shorter!

## Chrome Themes

Similar to the chrome content section above, we also use chrome themes (or `skin` URLs) in the DevTools UI. These are typically used to load CSS and images.

### Packaging

If you add a new file that should be loaded via `chrome://` (such as a new CSS file for a tool UI), you need to update a manifest file at `/devtools/client/jar.mn` so that it's packaged correctly.

Please ensure that any new files are added so their entire source tree path is part of the URL. To do so, the `jar.mn` entry should look like:

```
skin/<X> (themes/<X>)
```

where `<X>` is the path to your file after removing the `/devtools/client/themes/` prefix.

Example:

* File: `/devtools/client/themes/images/add.svg`
* Entry: `skin/images/add.svg (themes/images/add.svg)`

### Usage

Chrome theme URLs almost match their source tree path, with one difference: the segment `client/themes` is replaced by `skin`. This is a requirement of the `chrome://` protocol handler.

Example:

* File: `/devtools/client/themes/images/add.svg`
* Usage: `chrome://devtools/skin/images/add.svg`

## Localization (l10n)

Similar to the other chrome sections above, we also use `locale` URLs in the DevTools UI to load localized strings. This section applies to `*.dtd` (for use as entities within XUL / XHTML files) and `*.properties` (for use via runtime APIs) files.

We currently have two sets of localized files:

* `devtools/client/locales`: Strings used in the DevTools client (front-end UI)
* `devtools/shared/locales`: Strings used in either the DevTools server only, or shared with both the client and server

### Packaging

If you add a new l10n file (such as a new `*.dtd` or `*.properties` file), there should not be any additional packaging steps to perform, assuming the new file is placed in either of the 2 directories mentioned above. Each one contains a `jar.mn` which uses wildcards to package all files in the directory by default.

### Usage

Locale URLs differ somewhat based on whether they are in `client` or `shared`. While we would have preferred them to match the source tree path, the requirements of the `chrome://` protocol don't make that easy to do.

Example:

* File: `/devtools/client/locales/en-US/debugger.dtd`
* Usage: `chrome://devtools/locale/debugger.dtd`

Example:

* File: `/devtools/shared/locales/en-US/screenshot.properties`
* Usage: `chrome://devtools-shared/locale/screenshot.properties`

### Guidelines

Localization files should follow a set of guidelines aimed at making it easier for people to translate the labels in these files in many languages.

[Read best practices for developers](https://mozilla-l10n.github.io/documentation/localization/dev_best_practices.html).

In particular, it's important to write self-explanatory comments for new keys, deleting unused keys, changing the key name when changing the meaning of a string, and more. So make sure you read through these guidelines should you have to modify a localization file in your patch.
