<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Felipe Gomes [felipc@gmail.com] -->

The `page-worker` module provides a way to create a permanent, invisible page
and access its DOM.

The module exports a constructor function `Page`, which constructs a new page
worker.  A page worker may be destroyed, after which its memory is freed, and
you must create a new instance to load another page.

You specify the page to load using the `contentURL` option to the
[`Page()` constructor](modules/sdk/page-worker.html#Page(options)).
This can point to a remote file:

    pageWorker = require("sdk/page-worker").Page({
      contentScript: "console.log(document.body.innerHTML);",
      contentURL: "http://en.wikipedia.org/wiki/Internet"
    });

It can also point to an HTML file which you've packaged with your add-on.
To do this, save the file in your add-on's `data` directory and create the
URL using the `data.url()` method of the
[`self`](modules/sdk/self.html) module:

    pageWorker = require("sdk/page-worker").Page({
      contentScript: "console.log(document.body.innerHTML);",
      contentURL: require("sdk/self").data.url("myFile.html")
    });

You can load a new page by setting the page worker's `contentURL` property.
In this example we fetch the first paragraph of a page from Wikipedia, then
the first paragraph of a different page:

    var getFirstParagraph = "var paras = document.getElementsByTagName('p');" +
                            "console.log(paras[0].textContent);" +
                            "self.port.emit('loaded');"

    pageWorker = require("sdk/page-worker").Page({
      contentScript: getFirstParagraph,
      contentURL: "http://en.wikipedia.org/wiki/Chalk"
    });

    pageWorker.port.on("loaded", function() {
      pageWorker.contentURL = "http://en.wikipedia.org/wiki/Cheese"
    });

## Scripting Page-Worker Content ##

To access the page's DOM you need to attach a script to it. In the SDK these
scripts are called "content scripts" because they're explicitly used for
interacting with web content.

You can specify one or more content scripts to load into the page using the
`contentScript` or `contentScriptFile` options to the
[`Page()` constructor](modules/sdk/page-worker.html#Page(options)).
With `contentScript` you pass the script as a string, as in the examples
above. With `contentScriptFile` you pass a URL which points to a script
saved under your add-on's `data` directory. You construct the URL using
the `data.url()` method of the
[`self`](modules/sdk/self.html) module.

While content scripts can access DOM content, they can't access any of the SDK
APIs, so in many cases you'll need to exchange messages between the content
script and your main add-on code for a complete solution.

For example, the content script might read some content and send it back to
the main add-on, which could store it using the
[`simple-storage`](modules/sdk/simple-storage.html) API. You can
communicate with the script using either the
[`postMessage()`](dev-guide/guides/content-scripts/using-postmessage.html)
API or (preferably, usually) the
[`port`](dev-guide/guides/content-scripts/using-port.html) API.

For example, this add-on loads a page from Wikipedia, and runs a content script
in it to send all the headers back to the main add-on code:

    var pageWorkers = require("sdk/page-worker");

    // This content script sends header titles from the page to the add-on:
    var script = "var elements = document.querySelectorAll('h2 > span'); " +
                 "for (var i = 0; i < elements.length; i++) { " +
                 "  postMessage(elements[i].textContent) " +
                 "}";

    // Create a page worker that loads Wikipedia:
    pageWorkers.Page({
      contentURL: "http://en.wikipedia.org/wiki/Internet",
      contentScript: script,
      contentScriptWhen: "ready",
      onMessage: function(message) {
        console.log(message);
      }
    });

For conciseness, this example creates the content script as a string and uses
the `contentScript` property. In your own add-ons, you will probably want to
create your content scripts in separate files and pass their URLs using the
`contentScriptFile` property.

<div class="warning">
<p>Unless your content script is extremely simple and consists only of a
static string, don't use <code>contentScript</code>: if you do, you may
have problems getting your add-on approved on AMO.</p>
<p>Instead, keep the script in a separate file and load it using
<code>contentScriptFile</code>. This makes your code easier to maintain,
secure, debug and review.</p>
</div>

To learn much more about content scripts, see the
[Working with Content Scripts](dev-guide/guides/content-scripts/index.html)
guide.

<h3>Scripting Trusted Page Content</h3>

We've already seen that you can package HTML files in your add-on's `data`
directory and load them using `page-worker`. We can call this "trusted"
content, because unlike content loaded from a source outside the
add-on, the add-on author knows exactly what it's doing. To
interact with trusted content you don't need to use content scripts:
you can just include a script from the HTML file in the normal way, using
`<script>` tags.

Like a content script, these scripts can communicate with the add-on code
using the
[`postMessage()`](dev-guide/guides/content-scripts/using-postmessage.html)
API or the
[`port`](dev-guide/guides/content-scripts/using-port.html) API.
The crucial difference is that these scripts access the `postMessage`
and `port` objects through the `addon` object, whereas content scripts
access them through the `self` object.

So given an add-on that loads trusted content and uses content scripts
to access it, there are typically three changes you have to make, if you
want to use normal page scripts instead:

* **in the content script**: change occurrences of `self` to `addon`.
For example, `self.port.emit("my-event")` becomes
`addon.port.emit("my-event")`.

* **in the HTML page itself**: add a `<script>` tag to load the script. So
if your content script is saved under `data` as "my-script.js", you need
a line like `<script src="my-script.js"></script>` in the page header.

* **in the "main.js" file**: remove the `contentScriptFile` option in
the `Page()` constructor.

<api name="Page">
@class
A `Page` object loads the page specified by its `contentURL` option and
executes any content scripts that have been supplied to it in the
`contentScript` and `contentScriptFile` options.

The page is not displayed to the user.

The page worker is loaded as soon as the `Page` object is created and stays
loaded until its `destroy` method is called or the add-on is unloaded.

<api name="Page">
@constructor
  Creates an uninitialized page worker instance.
@param [options] {object}
  The *`options`* parameter is optional, and if given it should be an object
  with any of the following keys:
  @prop [contentURL] {string}
    The URL of the content to load in the panel.
  @prop [allow] {object}
    An object with keys to configure the permissions on the page worker. The
    boolean key `script` controls if scripts from the page are allowed to run.
    `script` defaults to true.
  @prop [contentScriptFile] {string,array}
    A local file URL or an array of local file URLs of content scripts to load.
    Content scripts specified by this option are loaded *before* those specified
    by the `contentScript` option.  See
    [Working with Content Scripts](dev-guide/guides/content-scripts/index.html)
    for help on setting this property.
  @prop [contentScript] {string,array}
    A string or an array of strings containing the texts of content scripts to
    load.  Content scripts specified by this option are loaded *after* those
    specified by the `contentScriptFile` option.
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

  @prop [onMessage] {function}
    Use this to add a listener to the page worker's `message` event.
</api>

<api name="port">
@property {EventEmitter}
[EventEmitter](modules/sdk/deprecated/events.html) object that allows you to:

* send events to the content script using the `port.emit` function
* receive events from the content script using the `port.on` function

See the guide to
<a href="dev-guide/guides/content-scripts/using-port.html">
communicating using <code>port</code></a> for details.
</api>

<api name="contentURL">
@property {string}
The URL of content to load.  This can point to
local content loaded from your add-on's "data" directory or remote content.
Setting it loads the content immediately.
</api>

<api name="allow">
@property {object}
  A object describing permissions for the content.  It contains a single key
  named `script` whose value is a boolean that indicates whether or not to
  execute script in the content.  `script` defaults to true.
</api>

<api name="contentScriptFile">
@property {string,array}
A local file URL or an array of local file URLs of content scripts to load.
</api>

<api name="contentScript">
@property {string,array}
A string or an array of strings containing the texts of content scripts to
load.
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

<api name="destroy">
@method
Unloads the page worker. After you destroy a page worker, its memory is freed
and you must create a new instance if you need to load another page.
</api>

<api name="postMessage">
@method
Sends a message to the content scripts.
@param message {value}
The message to send.  Must be JSON-able.
</api>

<api name="on">
@method
Registers an event listener with the page worker.  See
[Working with Events](dev-guide/guides/events.html) for help with
events.
@param type {string}
The type of event to listen for.
@param listener {function}
The listener function that handles the event.
</api>

<api name="removeListener">
@method
Unregisters an event listener from the page worker.
@param type {string}
The type of event for which `listener` was registered.
@param listener {function}
The listener function that was registered.
</api>

<api name="message">
@event
If you listen to this event you can receive message events from content
scripts associated with this page worker. When a content script posts a
message using `self.postMessage()`, the message is delivered to the add-on
code in the page worker's `message` event.

@argument {value}
Listeners are passed a single argument which is the message posted
from the content script. The message can be any
<a href = "dev-guide/guides/content-scripts/using-port.html#json_serializable">JSON-serializable value</a>
</api>

<api name="error">
@event
This event is emitted when an uncaught runtime error occurs in one of the
page worker's content scripts.

@argument {Error}
Listeners are passed a single argument, the
[Error](https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error)
object.
</api>

</api>
