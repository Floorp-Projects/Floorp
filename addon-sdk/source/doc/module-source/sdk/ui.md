<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Erik Vold [evold@mozilla.com] -->

This module exports a two constructor functions `Button` which constructs a
new toolbar button, and `Sidebar` which constructs a sidebar (with a button).

Sidebars are displayed on the left side of your browser. Its content is specified as
local HTML, so the appearance and behaviour of the sidebar
is limited only by what you can do using HTML, CSS and JavaScript.

The screenshot below shows a sidebar whose content is built from tweets:

<!-- add screen shot here -->

Sidebars are useful for presenting temporary interfaces to users in a way that is
easier for users to ignore and dismiss than a modal dialog 
and easier for users to keep around than a Panel, since sidebars are
displayed at the side of the browser until the user decides to close it.

A sidebar's content is loaded anew as soon as it is opened, and unloads
when the user closes the sidebar.

Your add-on can receive notifications when a sidebar is shown or hidden by
listening to its `show` and `hide` events.

Opening a sidebar in a window will close an already opened sidebar in that window.

## Sidebar Content ##

The sidebar's content is specified as HTML, which is loaded from the URL
supplied in the `url` option to the sidebar's constructor.

You can load remote HTML into the sidebar:

    var sidebar = require("sdk/ui").Sidebar({
      id: 'a-new-sidebar',
      title: 'A New Sidebar',
      icon: './icon.png',
      url: './index.html'
    });

    sidebar.show();

This will load HTML that's been packaged with your add-on, and this is
most probably how you will create sidebars. To do this, save
the `index.html` HTML file in your add-on's `data` directory.

## Sidebar Positioning ##

By default the sidebars appears on the left side of the currently active browser window.


## Updating Sidebar Content ##

You can update the sidebar's content simply by setting the sidebar's `url`
property.  Note this will change the sidebar's url for all windows.

## Scripting Sidebar Content ##

You can't directly access your sidebar's content from your main add-on code.
To access the sidebar's content, you need to add a `<script>` into the sidebar.

The sidebar's scripts will have access to a `addon` global, with you can
communicate with your main add-on code, like so:

`lib/main.js`:

    let sidebar = Sidebar({
      id: 'a-new-sidebar',
      title: 'A New Sidebar',
      icon: './icon.png',
      url: './index.html',
      onAttach: function (worker) {
        worker.port.on('message', function() {  // part 2
            // do things...
            worker.port.emit('message', 'I have information for you!');  // part 3
        });
      }
    });

`data/index.html`

<pre class="brush: html">
&lt;html&gt;
  &lt;head&gt;&lt;/head&gt;
  &lt;body&gt;
    ...
    &lt;script&gt;
    addon.port.on('message', function(msg) {  // part 4
      // msg will == 'I have information for you!'
    });
    // starting communication here..
    addon.port.emit('message');  // part 1
    &lt;/script&gt;
  &lt;/body&gt;
&lt;/html&gt;
</pre>

<api name="Sidebar">
@class
The Sidebar object.

Once a sidebar object has been created it can be shown and hidden,
in the active window, using its
`show()` and `hide()` methods. Once a sidebar is no longer needed it can be
removed/destructed using `destroy()`.

<api name="Sidebar">
@constructor
Creates a sidebar.
@param options {object}
  Options for the sidebar, with the following keys:
  @prop id {string}
    The `id` of the sidebar.
  @prop title {string}
    A title for the sidebar.
  @prop icon {string, object}
    The icon used for the sidebar's button.
  @prop url {string, URL}
    The URL of the content to load in the sidebar.
  @prop [onAttach] {function}
    Include this to listen to the sidebar's `attach` event.
  @prop [onShow] {function}
    Include this to listen to the sidebar's `show` event.
  @prop [onHide] {function}
    Include this to listen to the sidebar's `hide` event.
</api>

<api name="id">
@property {string}
The `id` for the sidebar.
</api>

<api name="title">
@property {string}
The `title` of the sidebar.
</api>

<api name="icon">
@property {string, object}
The global icon for the sidebar.
</api>

<api name="url">
@property {string}
The URL of content loaded into the sidebar. 
</api>

<api name="destroy">
@method
Destroys the sidebar, once destroyed, the sidebar can no longer be used.
</api>

<api name="show">
@method
Displays the sidebar in the active window.
</api>

<api name="hide">
@method
Hides the sidebar in the active window.
</api>

<api name="on">
@method
  Registers an event listener with the sidebar.
@param type {string}
  The type of event to listen for.
@param listener {function}
  The listener function that handles the event.
</api>

<api name="once">
@method
  Registers an event listener with the sidebar.
  The difference between `on` and `once` is that
  `on` will continue listening until it is
  removed, whereas `once` is removed automatically
  upon the first event it catches.
@param type {string}
  The type of event to listen for.
@param listener {function}
  The listener function that handles the event.
</api>

<api name="removeListener">
@method
  Unregisters/removes an event listener from the sidebar.
@param type {string}
  The type of event for which `listener` was registered.
@param listener {function}
  The listener function that was registered.
</api>

<api name="attach">
@event
This event is emitted when the sidebar's window
is created and the `addon` global was added.
</api>

<api name="show">
@event
This event is emitted when the sidebar is shown.
</api>

<api name="hide">
@event
This event is emitted when the sidebar is hidden.
</api>

</api>
