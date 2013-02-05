<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Irakli Gozalishvili [gozala@mozilla.com] -->

The `loader` module provides one of the building blocks for those modules
in the SDK that use
[content scripts to interact with web content](dev-guide/guides/content-scripts/index.html),
such as the [`panel`](modules/sdk/panel.html) and [`page-mod`](modules/sdk/page-mod.html)
modules.

The module exports a constructor for the `Loader` object, which is composed
from the [EventEmitter](modules/sdk/deprecated/events.html) trait, so it
inherits `on()`, `once()`, and `removeListener()` functions that
enable its users to listen to events.

`Loader` adds code to initialize and validate a set of properties for
managing content scripts:

* `contentURL`
* `contentScript`
* `contentScriptFile`
* `contentScriptWhen`
* `contentScriptOptions`
* `allow`

When certain of these properties are set, the `Loader` emits a
`propertyChange` event, enabling its users to take the appropriate action.

The `Loader` is used by modules that use content scripts but don't
themselves load content, such as [`page-mod`](modules/sdk/page-mod.html).

Modules that load their own content, such as
[`panel`](modules/sdk/panel.html), [`page-worker`](modules/sdk/page-worker.html), and
[`widget`](modules/sdk/widget.html), use the
[`symbiont`](modules/sdk/content/symbiont.html) module instead.
`Symbiont` inherits from `Loader` but contains its own frame into which
it loads content supplied as the `contentURL` option.

**Example:**

The following code creates a wrapper on a hidden frame that reloads a web page
in the frame every time the `contentURL` property is changed:

    var hiddenFrames = require("sdk/frame/hidden-frame");
    var { Loader } = require("sdk/content/content");
    var PageLoader = Loader.compose({
      constructor: function PageLoader(options) {
        options = options || {};
        if (options.contentURL)
          this.contentURL = options.contentURL;
        this.on('propertyChange', this._onChange = this._onChange.bind(this));
        let self = this;
        hiddenFrames.add(hiddenFrames.HiddenFrame({
          onReady: function onReady() {
            let frame = self._frame = this.element;
            self._emit('propertyChange', { contentURL: self.contentURL });
          }
        }));
      },
      _onChange: function _onChange(e) {
        if ('contentURL' in e)
          this._frame.setAttribute('src', this._contentURL);
      }
    });

<api name="Loader">
@class
<api name="contentScriptFile">
@property {array}
The local file URLs of content scripts to load.  Content scripts specified by
this property are loaded *before* those specified by the `contentScript`
property.
</api>

<api name="contentScript">
@property {array}
The texts of content scripts to load.  Content scripts specified by this
property are loaded *after* those specified by the `contentScriptFile` property.
</api>

<api name="contentScriptWhen">
@property {string}
When to load the content scripts. This may take one of the following
values:

* "start": load content scripts immediately after the document
element for the page is inserted into the DOM, but before the DOM content
itself has been loaded
* "ready": load content scripts once DOM content has been loaded,
corresponding to the
[DOMContentLoaded](https://developer.mozilla.org/en/Gecko-Specific_DOM_Events)
event
* "end": load content scripts once all the content (DOM, JS, CSS,
images) for the page has been loaded, at the time the
[window.onload event](https://developer.mozilla.org/en/DOM/window.onload)
fires

</api>

<api name="contentScriptOptions">
@property {object}
Read-only value exposed to content scripts under `self.options` property.

Any kind of jsonable value (object, array, string, etc.) can be used here.
Optional.
</api>

<api name="contentURL">
@property {string}
The URL of the content loaded.
</api>

<api name="allow">
@property {object}
Permissions for the content, with the following keys:
@prop script {boolean}
  Whether or not to execute script in the content.  Defaults to true.
</api>
</api>

