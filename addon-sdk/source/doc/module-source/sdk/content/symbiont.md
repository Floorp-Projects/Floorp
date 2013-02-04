<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Myk Melez [myk@mozilla.org] -->
<!-- contributed by Irakli Gozalishvili [gozala@mozilla.com] -->

The `symbiont` module exports the `Symbiont` trait, which is used in
the internal implementation of SDK modules, such as
[`panel`](modules/sdk/panel.html) and
[`page-worker`](modules/sdk/page-mod.html), that can load web
content and attach
[content scripts](dev-guide/guides/content-scripts/index.html) to it.

A `Symbiont` loads the specified `contentURL` and content scripts into
a frame, and sets up an asynchronous channel between the content
scripts and the add-on code, enabling them to exchange messages using the
[`port`](dev-guide/guides/content-scripts/using-port.html) or
[`postMessage`](dev-guide/guides/content-scripts/using-postmessage.html)
APIs. You map optionally pass a frame into the `Symbiont`'s constructor:
if you don't, then a new hidden frame will be created to host the content.

This trait is composed from the
[`Loader`](modules/sdk/content/loader.html) and
[`Worker`](modules/sdk/content/worker.html) traits. It inherits
functions to load and configure content scripts from the `Loader`,
and functions to exchange messages between content scripts and the
main add-on code from the `Worker`.

    var { Symbiont } = require('sdk/content/content');
    var Thing = Symbiont.resolve({ constructor: '_init' }).compose({
      constructor: function Thing(options) {
        // `getMyFrame` returns the host application frame in which
        // the page is loaded.
        this._frame = getMyFrame();
        this._init(options)
      }
    });

See the [panel][] module for a real-world example of usage of this module.

[panel]:modules/sdk/panel.html

<api name="Symbiont">
@class
Symbiont is composed from the [Worker][] trait, therefore instances
of Symbiont and their descendants expose all the public properties
exposed by [Worker][] along with additional public properties that
are listed below:

[Worker]:modules/sdk/content/worker.html

<api name="Symbiont">
@constructor
Creates a content symbiont.
@param options {object}
  Options for the constructor. Includes all the keys that
the [Worker](modules/sdk/content/worker.html)
constructor accepts and a few more:

  @prop [frame] {object}
    The host application frame in which the page is loaded.
    If frame is not provided hidden one will be created.
  @prop [contentScriptWhen="end"] {string}
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

    This property is optional and defaults to "end".
  @prop [contentScriptOptions] {object}
    Read-only value exposed to content scripts under `self.options` property.

    Any kind of jsonable value (object, array, string, etc.) can be used here.
    Optional.

  @prop [allow] {object}
    Permissions for the content, with the following keys:
      @prop [script] {boolean}
      Whether or not to execute script in the content.  Defaults to true.
      Optional.
    Optional.
</api>

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
When to load the content scripts. This may have one of the following
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
Permissions for the content, with a single boolean key called `script` which
defaults to true and indicates whether or not to execute scripts in the
content.
</api>

</api>


