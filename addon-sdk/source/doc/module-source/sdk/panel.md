<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Myk Melez [myk@mozilla.org] -->
<!-- contributed by Irakli Gozalishvili [gozala@mozilla.com] -->

This module exports a single constructor function `Panel` which constructs a
new panel.

A panel is a dialog. Its content is specified as HTML and you can
execute scripts in it, so the appearance and behaviour of the panel
is limited only by what you can do using HTML, CSS and JavaScript.

The screenshot below shows a panel whose content is built from the
list of currently open tabs:

<img class="image-center" src="static-files/media/screenshots/panel-tabs-osx.png"
alt="Simple panel example">

Panels are useful for presenting temporary interfaces to users in a way that is
easier for users to ignore and dismiss than a modal dialog, since panels are
hidden the moment users interact with parts of the application interface outside
them.

A panel's content is loaded as soon as it is created, before the panel is shown,
and the content remains loaded when a panel is hidden, so it is possible
to keep a panel around in the background, updating its content as appropriate
in preparation for the next time it is shown.

Your add-on can receive notifications when a panel is shown or hidden by
listening to its `show` and `hide` events.

Opening a panel will close an already opened panel.

## Panel Content ##

The panel's content is specified as HTML, which is loaded from the URL
supplied in the `contentURL` option to the panel's constructor.

You can load remote HTML into the panel:

    var panel = require("sdk/panel").Panel({
      width: 180,
      height: 180,
      contentURL: "https://en.wikipedia.org/w/index.php?title=Jetpack&useformat=mobile"
    });

    panel.show();

<img class="image-center" src="static-files/media/screenshots/wikipedia-jetpack-panel.png"
alt="Wikipedia Jetpack panel">

You can also load HTML that's been packaged with your add-on, and this is
most probably how you will create dialogs. To do this, save
the HTML in your add-on's `data` directory and load it using the `data.url()`
method exported by the
[`self`](modules/sdk/self.html) module, like this:

    var panel = require("sdk/panel").Panel({
      contentURL: require("sdk/self").data.url("myFile.html")
    });

    panel.show();

## Panel Positioning ##

By default the panel appears in the center of the currently active browser window.
You can position the panel by passing a `position` to the panel's
[constructor](modules/sdk/panel.html#Panel(options)) or to
its [`show()`](modules/sdk/panel.html#show(options)) method.

## Updating Panel Content ##

You can update the panel's content simply by setting the panel's `contentURL`
property.

Here's an add-on that adds two widgets to the add-on bar, one which
shows Google's mobile site and one which shows Bing's mobile site. The widgets
share a panel object, and switch between the two sites by updating the panel's
`contentURL` property:

    var panel = require("sdk/panel").Panel({
      contentURL: "about:blank",
      onHide: function () {
        panel.contentURL = "about:blank";
      }
    });

    require("sdk/widget").Widget({
      id: "bing",
      label: "Bing",
      contentURL: "http://www.bing.com/favicon.ico",
      panel: panel,
      onClick: function() {
        panel.contentURL = "http://m.bing.com/";
      }
    });

    require("sdk/widget").Widget({
      id: "google",
      label: "Google",
      contentURL: "http://www.google.com/favicon.ico",
      panel: panel,
      onClick: function() {
        panel.contentURL = "http://www.google.com/xhtml";
      }
    });

## Scripting Panel Content ##

You can't directly access your panel's content from your main add-on code.
To access the panel's content, you need to load a script into the panel.
In the SDK these scripts are called "content scripts" because they're
explicitly used for interacting with web content.

While content scripts can access the content they're attached to, they can't
use the SDK's APIs. So implementing a complete solution usually means you
have to send messages between the content script and the main add-on code.

* You can specify one or more content scripts to load into a panel using the
`contentScript` or `contentScriptFile` options to the
[`Panel()` constructor](modules/sdk/panel.html#Panel%28options%29).

* You can communicate with the script using either the
[`postMessage()`](dev-guide/guides/content-scripts/using-postmessage.html)
API or (preferably, usually) the
[`port`](dev-guide/guides/content-scripts/using-port.html) API.

For example, here's an add-on whose content script intercepts mouse clicks
on links inside the panel, and sends the target URL to the main add-on
code. The content script sends messages using `self.port.emit()` and the
add-on script receives them using `panel.port.on()`.

    var myScript = "window.addEventListener('click', function(event) {" +
                   "  var t = event.target;" +
                   "  if (t.nodeName == 'A')" +
                   "    self.port.emit('click-link', t.toString());" +
                   "}, false);"

    var panel = require("sdk/panel").Panel({
      contentURL: "http://www.bbc.co.uk/mobile/index.html",
      contentScript: myScript
    });

    panel.port.on("click-link", function(url) {
      console.log(url);
    });

    panel.show();

This example uses `contentScript` to supply the script as a string. It's
usually better practice to use `contentScriptFile`, which is a URL pointing
to a script file saved under your add-on's `data` directory.

<div class="warning">
<p>Unless your content script is extremely simple and consists only of a
static string, don't use <code>contentScript</code>: if you do, you may
have problems getting your add-on approved on AMO.</p>
<p>Instead, keep the script in a separate file and load it using
<code>contentScriptFile</code>. This makes your code easier to maintain,
secure, debug and review.</p>
</div>

<img class="image-right" src="static-files/media/screenshots/text-entry-panel.png"
alt="Text entry panel">

### Getting User Input ###

The following add-on adds a widget which displays a panel when
clicked. The panel just contains a `<textarea>` element: when the user
presses the `return` key, the contents of the `<textarea>` is sent to the
main add-on code.

The add-on consists of three files:

* **`main.js`**: the main add-on code, that creates the widget and panel
* **`get-text.js`**: the content script that interacts with the panel content
* **`text-entry.html`**: the panel content itself, specified as HTML

"main.js" is saved in your add-on's `lib` directory, and the other two files
go in your add-on's `data` directory:

<pre>
my-addon/
         data/
              get-text.js
              text-entry.html
         lib/
             main.js
</pre>

The "main.js" looks like this:

    var data = require("sdk/self").data;

    // Create a panel whose content is defined in "text-entry.html".
    // Attach a content script called "get-text.js".
    var text_entry = require("sdk/panel").Panel({
      width: 212,
      height: 200,
      contentURL: data.url("text-entry.html"),
      contentScriptFile: data.url("get-text.js")
    });

    // Send the content script a message called "show" when
    // the panel is shown.
    text_entry.on("show", function() {
      text_entry.port.emit("show");
    });

    // Listen for messages called "text-entered" coming from
    // the content script. The message payload is the text the user
    // entered.
    // In this implementation we'll just log the text to the console.
    text_entry.port.on("text-entered", function (text) {
      console.log(text);
      text_entry.hide();
    });

    // Create a widget, and attach the panel to it, so the panel is
    // shown when the user clicks the widget.
    require("sdk/widget").Widget({
      label: "Text entry",
      id: "text-entry",
      contentURL: "http://www.mozilla.org/favicon.ico",
      panel: text_entry
    });

The content script "get-text.js" looks like this:

    self.port.on("show", function (arg) {
      var textArea = document.getElementById('edit-box');
      textArea.focus();
      // When the user hits return, send a message to main.js.
      // The message payload is the contents of the edit box.
      textArea.onkeyup = function(event) {
        if (event.keyCode == 13) {
          // Remove the newline.
          text = textArea.value.replace(/(\r\n|\n|\r)/gm,"");
          self.port.emit("text-entered", text);
          textArea.value = '';
        }
      };
    });

Finally, the "text-entry.html" file defines the `<textarea>` element:

<pre class="brush: html">

&lt;html&gt;

&lt;head&gt;
  &lt;meta charset="utf-8"&gt;
  &lt;style type="text/css" media="all"&gt;
    textarea {
      margin: 10px;
    }
  &lt;/style&gt;
&lt;/head&gt;

&lt;body&gt;
  &lt;textarea rows="10" cols="20" id="edit-box">&lt;/textarea&gt;
&lt;/body&gt;

&lt;/html&gt;
</pre>

To learn much more about content scripts, see the
[Working with Content Scripts](dev-guide/guides/content-scripts/index.html)
guide.

<h3>Scripting Trusted Panel Content</h3>

We've already seen that you can package HTML files in your add-on's `data`
directory and use them to define the panel's content. We can call this
"trusted" content, because unlike content loaded from a source outside the
add-on, the add-on author knows exactly what it's doing. To
interact with trusted content you don't need to use content scripts:
you can just include a script from the HTML file in the normal way, using
`script` tags.

Like a content script, these scripts can communicate with the add-on code
using the
[`postMessage()`](dev-guide/guides/content-scripts/using-postmessage.html)
API or the
[`port`](dev-guide/guides/content-scripts/using-port.html) API.
The crucial difference is that these scripts access the `postMessage`
and `port` objects through the `addon` object, whereas content scripts
access them through the `self` object.

To show the difference, we can easily convert the `text-entry` add-on above
to use normal page scripts instead of content scripts.

The main add-on code is exactly the same as the main add-on code in the
previous example, except that we don't attach a content script:

    var data = require("sdk/self").data;

    // Create a panel whose content is defined in "text-entry.html".
    var text_entry = require("sdk/panel").Panel({
      width: 212,
      height: 200,
      contentURL: data.url("text-entry.html"),
    });

    // Send the page script a message called "show" when
    // the panel is shown.
    text_entry.on("show", function() {
      text_entry.port.emit("show");
    });

    // Listen for messages called "text-entered" coming from
    // the page script. The message payload is the text the user
    // entered.
    // In this implementation we'll just log the text to the console.
    text_entry.port.on("text-entered", function (text) {
      console.log(text);
      text_entry.hide();
    });

    // Create a widget, and attach the panel to it, so the panel is
    // shown when the user clicks the widget.
    require("sdk/widget").Widget({
      label: "Text entry",
      id: "text-entry",
      contentURL: "http://www.mozilla.org/favicon.ico",
      panel: text_entry
    });

The page script is exactly the same as the content script above, except
that instead of `self`, we use `addon` to access the messaging APIs:

    addon.port.on("show", function (arg) {
      var textArea = document.getElementById('edit-box');
      textArea.focus();
      // When the user hits return, send a message to main.js.
      // The message payload is the contents of the edit box.
      textArea.onkeyup = function(event) {
        if (event.keyCode == 13) {
          // Remove the newline.
          text = textArea.value.replace(/(\r\n|\n|\r)/gm,"");
          addon.port.emit("text-entered", text);
          textArea.value = '';
        }
      };
    });

Finally, the HTML file now references "get-text.js" inside a `script` tag:

<pre class="brush: html">

&lt;html&gt;

&lt;head&gt;
  &lt;style type="text/css" media="all"&gt;
    textarea {
      margin: 10px;
    }
  &lt;/style&gt;
  &lt;script src="get-text.js"&gt;&lt;/script&gt;
&lt;/head&gt;

&lt;body&gt;
  &lt;textarea rows="10" cols="20" id="edit-box">&lt;/textarea&gt;
&lt;/body&gt;

&lt;/html&gt;
</pre>

## Styling Trusted Panel Content ##

When the panel's content is specified using an HTML file in your `data`
directory, you can style it using CSS, either embedding the CSS directly
in the file or referencing a CSS file stored under `data`.

The panel's default style is different for each operating system:

<img class="image-center" src="static-files/media/screenshots/panel-default-style.png"
alt="OS X panel default style">

This helps to ensure that the panel's style is consistent with the dialogs
displayed by Firefox and other applications, but means you need to take care
when applying your own styles. For example, if you set the panel's
`background-color` property to `white` and do not set the `color` property,
then the panel's text will be invisible on OS X although it looks fine on Ubuntu.

## Private Browsing ##

If your add-on has not
[opted into private browsing](modules/sdk/private-browsing.html#Opting into private browsing),
and it calls `panel.show()` when the currently active window is a
[private window](modules/sdk/private-browsing.html#Per-window private browsing),
then the panel will not be shown.

<api name="Panel">
@class
The Panel object represents a floating modal dialog that can by an add-on to
present user interface content.

Once a panel object has been created it can be shown and hidden using its
`show()` and `hide()` methods. Once a panel is no longer needed it can be
deactivated using `destroy()`.

The content of a panel is specified using the `contentURL` option. An add-on
can interact with the content of a panel using content scripts which it
supplies in the `contentScript` and/or `contentScriptFile` options. For example,
a content script could create a menu and send the user's selection to the
add-on.

<api name="Panel">
@constructor
Creates a panel.
@param options {object}
  Options for the panel, with the following keys:
  @prop [width] {number}
    The width of the panel in pixels. Optional.
  @prop [height] {number}
    The height of the panel in pixels. Optional.
  @prop [position] {object}
    The position of the panel.
    Ignored if the panel is opened by a widget.

    This is an object that has one or more of the following
    properties: `top`, `right`, `bottom` and `left`. Their values are expressed
    in pixels. Any other properties will be ignored.

    The default alignment along each axis is centered: so to display a panel centred
    along the vertical or horizontal axis, just omit that axis:

        // Show the panel centered horizontally and
        // aligned to the bottom of the content area
        require("sdk/panel").Panel({
          position: {
           bottom: 0
          }
        }).show();

        // Show the panel centered vertically and
        // aligned to the left of the content area
        require("sdk/panel").Panel({
          position: {
            left: 0
          }
        }).show();

        // Centered panel, default behavior
        require("sdk/panel").Panel({}).show();

    As with the CSS `top`, `bottom`, `left`, and `right` properties, setting
    both `top` and `bottom` or both `left` and `right` will implicitly set the
    panel's `height` or `width` relative to the content window:

        // Show the panel centered horizontally, with:
        // - the top edge 40px from the top of the content window
        // - the bottom edge 100px from the bottom of the content window
        require("sdk/panel").Panel({
          position: {
            top: 40,
            bottom: 100
          }
        }).show();

    If you set both `top` and `bottom`, but also set the panel's height
    explicitly using the `height` property, then the panel will ignore
    `bottom`, just as CSS does for its properties with the same name:

        // Show the panel centered horizontally, with:
        // - the top edge 40px from the top of the content window
        // - a height of 400px
        require("sdk/panel").Panel({
          position: {
            top: 40,
            bottom: 100,
          },
          height: 400
        }).show();

        // This is equivalent to:

        require("panel").Panel({
          position {
            top: 40
          },
          height: 400
        }).show();

    The same principle is applied in the horizontal axis with
    `width`, `left` and `right`.

  @prop [focus=true] {boolean}
    Set to `false` to prevent taking the focus away when the panel is shown.
    Only turn this off if necessary, to prevent accessibility issue.
    Optional, default to `true`.
  @prop [contentURL] {string,URL}
    The URL of the content to load in the panel.
  @prop [allow] {object}
    An optional object describing permissions for the content.  It should
    contain a single key named `script` whose value is a boolean that indicates
    whether or not to execute script in the content.  `script` defaults to true.
  @prop [contentScriptFile] {string,array}
    A local file URL or an array of local file URLs of content scripts to load.
    Content scripts specified by this property are loaded *before* those
    specified by the `contentScript` property.
  @prop [contentScript] {string,array}
    A string or an array of strings containing the texts of content scripts to
    load.  Content scripts specified by this property are loaded *after* those
    specified by the `contentScriptFile` property.
  @prop [contentScriptWhen="end"] {string}
    When to load the content scripts. This may take one of the following
    values:

    * "start": load content scripts immediately after the document
    element for the panel is inserted into the DOM, but before the DOM content
    itself has been loaded
    * "ready": load content scripts once DOM content has been loaded,
    corresponding to the
    [DOMContentLoaded](https://developer.mozilla.org/en/Gecko-Specific_DOM_Events)
    event
    * "end": load content scripts once all the content (DOM, JS, CSS,
    images) for the panel has been loaded, at the time the
    [window.onload event](https://developer.mozilla.org/en/DOM/window.onload)
    fires

    This property is optional and defaults to "end".
  @prop [contentScriptOptions] {object}
    Read-only value exposed to content scripts under `self.options` property.

    Any kind of jsonable value (object, array, string, etc.) can be used here.
    Optional.

  @prop [onMessage] {function}
    Include this to listen to the panel's `message` event.
  @prop [onShow] {function}
    Include this to listen to the panel's `show` event.
  @prop [onHide] {function}
    Include this to listen to the panel's `hide` event.
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

<api name="isShowing">
@property {boolean}
Tells if the panel is currently shown or not. This property is read-only.
</api>

<api name="height">
@property {number}
The height of the panel in pixels.
</api>

<api name="width">
@property {number}
The width of the panel in pixels.
</api>

<api name="focus">
@property {boolean}
Whether of not focus will be taken away when the panel is shown.
This property is read-only.
</api>

<api name="contentURL">
@property {string}
The URL of content loaded into the panel.  This can point to
local content loaded from your add-on's "data" directory or remote content.
Setting it updates the panel's content immediately.
</api>

<api name="allow">
@property {object}
An object describing permissions for the content.  It contains a single key
named `script` whose value is a boolean that indicates whether or not to execute
script in the content.
</api>

<api name="contentScriptFile">
@property {string,array}
A local file URL or an array of local file URLs of content scripts to load.
Content scripts specified by this property are loaded *before* those
specified by the `contentScript` property.
</api>

<api name="contentScript">
@property {string,array}
A string or an array of strings containing the texts of content scripts to
load.  Content scripts specified by this property are loaded *after* those
specified by the `contentScriptFile` property.
</api>

<api name="contentScriptWhen">
@property {string}
When to load the content scripts. This may have one of the following
values:

* "start": load content scripts immediately after the document
element for the panel is inserted into the DOM, but before the DOM content
itself has been loaded
* "ready": load content scripts once DOM content has been loaded,
corresponding to the
[DOMContentLoaded](https://developer.mozilla.org/en/Gecko-Specific_DOM_Events)
event
* "end": load content scripts once all the content (DOM, JS, CSS,
images) for the panel has been loaded, at the time the
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
Destroys the panel, unloading any content that was loaded in it. Once
destroyed, the panel can no longer be used. If you just want to hide
the panel and might show it later, use `hide` instead.
</api>

<api name="postMessage">
@method
Sends a message to the content scripts.
@param message {value}
The message to send.  Must be stringifiable to JSON.
</api>

<api name="show">
@method
Displays the panel.

If the `options` argument is given, it will be shallow merged with the options
provided in the constructor: the `options` passed in the `show` method takes
precedence.

Passing options here is useful for making temporary changes without touching
the default values.

@param options {object}
  Showing options for the panel, with the following keys:
  @prop [width] {number}
    The width of the panel in pixels. Optional.
  @prop [height] {number}
    The height of the panel in pixels. Optional.
  @prop [position] {object}
    The position of the panel. Optional. See [Panel's options](./modules/sdk/panel.html#Panel%28options%29) for further details.
  @prop [focus=true] {boolean}
    Set to `false` to prevent taking the focus away when the panel is shown.
</api>

<api name="hide">
@method
Stops displaying the panel.
</api>

<api name="resize">
@method
Resizes the panel.
@param width {number}
The new width of the panel in pixels.
@param height {number}
The new height of the panel in pixels.
</api>

<api name="on">
@method
  Registers an event listener with the panel.
@param type {string}
  The type of event to listen for.
@param listener {function}
  The listener function that handles the event.
</api>

<api name="removeListener">
@method
  Unregisters an event listener from the panel.
@param type {string}
  The type of event for which `listener` was registered.
@param listener {function}
  The listener function that was registered.
</api>

<api name="show">
@event
This event is emitted when the panel is shown.
</api>

<api name="hide">
@event
This event is emitted when the panel is hidden.
</api>

<api name="message">
@event
If you listen to this event you can receive message events from content
scripts associated with this panel. When a content script posts a
message using `self.postMessage()`, the message is delivered to the add-on
code in the panel's `message` event.

@argument {value}
Listeners are passed a single argument which is the message posted
from the content script. The message can be any
<a href = "dev-guide/guides/content-scripts/using-port.html#json_serializable">JSON-serializable value</a>.
</api>

<api name="error">
@event
This event is emitted when an uncaught runtime error occurs in one of the
panel's content scripts.

@argument {Error}
Listeners are passed a single argument, the
[Error](https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error)
object.
</api>

</api>
