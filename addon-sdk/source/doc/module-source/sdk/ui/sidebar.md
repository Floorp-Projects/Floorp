<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `sidebar` module enables you to create sidebars. A sidebar is a a strip
of user interface real estate for your add-on that's attached to the
left-hand side of the browser window. A sidebar is a bit like a widget,
in that it's a persistent user interface element belonging to your add-on
that's attached to the browser window. However, it's much bigger than a
widget and is suitable for more presenting more complex interfaces.

## Creating, showing, and hiding sidebars

You construct a `Sidebar` object using the [`Sidebar()`](modules/sdk/ui/sidebar.html#Sidebar(options)) constructor.

Once you've done that, you can show the sidebar in the currently active window
by calling the Sidebar's [`show()`](modules/sdk/ui/sidebar.html#show()) method. If a new window is opened from a
window that has a sidebar visible, the new window gets a sidebar, too.

You can hide the sidebar in the currently active window by calling its
[`hide()`](modules/sdk/ui/sidebar.html#hide()) method.

Alternatively, the View->Sidebar submenu in Firefox will contain a new item
which the user can use to show or hide the sidebar:

<img class="image-center" src="static-files/media/screenshots/sidebar-menu.png" alt="Opening sidebar from a menu">

The sidebar generates a [`show`](modules/sdk/ui/sidebar.html#show) event when it is shown and a [`hide`](modules/sdk/ui/sidebar.html#hide) event when
it is hidden.

Once you've finished using the sidebar you can destroy it by calling its
[`dispose()`](file:///Users/Work/mozilla/jetpack-sdk/doc/modules/sdk/ui/sidebar.html#dispose()) method.

To show what a sidebar looks like, here's a sidebar that displays the results
of running the [W3C Validator](http://validator.w3.org/) on the current page:

<img class="image-center" src="static-files/media/screenshots/sidebar.png" alt="Simple sidebar example">


## Specifying sidebar content

The content of a sidebar is specified using HTML, which is loaded from the URL
supplied in the `url` option to the sidebar's constructor. Unlike modules
such as [`panel`](modules/sdk/panel.html), the content must be local,
typically loaded from the add-on's `data` directory via a URL constructed
using [`self.data.url()`](modules/sdk/self.html#data):

    var sidebar = require("sdk/ui/sidebar").Sidebar({
      id: 'my-sidebar',
      title: 'My sidebar',
      url: require("sdk/self").data.url("sidebar.html")
    });

You can include JavaScript and CSS from the HTML as you would with any web
page, for example using `<script>` and `<link>` tags containing a path
relative to the HTML file itself.

<pre class="brush: html">

&lt;!DOCTYPE HTML&gt;
&lt;html&gt;
  &lt;head&gt;
    &lt;link href="stuff.css" type="text/css" rel="stylesheet"&gt;
  &lt;/head&gt;
  &lt;body&gt;
    &lt;script type="text/javascript" src="stuff.js"&gt;&lt;/script&gt;
  &lt;/body&gt;
&lt;/html&gt;

</pre>

You can update the sidebar's content by setting the sidebar's `url`
property. This will change the sidebar's content across all windows.

## Communicating with sidebar scripts ##

You can't directly access your sidebar's content from your main add-on code,
but you can send messages between your main add-on code and scripts loaded
into your sidebar.

On the sidebar end of the conversation, sidebar scripts get a global
variable `addon` that contains a `port` for sending and receiving
messages.

On the add-on side, you must listen for the sidebar's `attach` event,
which is triggered whenever the DOM for a new sidebar instance is loaded
and its scripts are attached. This message passes a `worker` object into
the listener function, and you can use that worker to communicate with the
script loaded into the new sidebar instance.

Here's a simple but complete add-on that shows how to set up communication
between main.js and a script in a sidebar:

The HTML file includes just a script, "sidebar.js":

<pre class="brush: html">

&lt;!DOCTYPE HTML&gt;
&lt;html&gt;
  &lt;body&gt;
    Content for my sidebar
    &lt;script type="text/javascript" src="sidebar.js"&gt;&lt;/script&gt;
  &lt;/body&gt;
&lt;/html&gt;

</pre>

The "sidebar.js" file sends a `ready` message to main.js using `port.emit()`,
and adds a listener to the `init` message that logs the message payload.

    addon.port.emit("ready");
    addon.port.on("init", sayStuff);

    function sayStuff(message) {
      console.log("got the message '" + message + "'" );
    }

The "main.js" file creates a sidebar object and adds a listener to its
`attach` event. The listener listens for the `ready` message from the
sidebar script, and responds with an `init` message:

    var sidebar = require("sdk/ui/sidebar").Sidebar({
      id: 'my-sidebar',
      title: 'My sidebar',
      url: require("sdk/self").data.url("sidebar.html"),
      onAttach: function (worker) {
        worker.port.on("ready", function() {
          worker.port.emit("init", "message from main.js");
        });
      }
    });

Try running the add-on, and showing the sidebar using the
"View->Sidebar->My sidebar" menu item. You should see console output like:

<pre>
example-add-on: got the message 'message from main.js'
</pre>

You *can't* send the sidebar script
messages in the `attach` event listener, because the script may not be
initialized yet. It's safer, as in this example, to have the script
message "main.js" as soon as it is loaded, and have "main.js" listen
for that message before responding.

<api name="Sidebar">
@class
The Sidebar object. Once a sidebar has been created it can be shown and hidden
in the active window using its
[`show()`](modules/sdk/ui/sidebar.html#show()) and
[`hide()`](modules/sdk/ui/sidebar.html#hide()) methods.
Once a sidebar is no longer needed it can be
destroyed using [`dispose()`](modules/sdk/ui/sidebar.html#dispose()).

<api name="Sidebar">
@constructor
Creates a sidebar.

     var sidebar = require("sdk/ui/sidebar").Sidebar({
    id: 'my-sidebar',
    title: 'My sidebar',
    url: require("sdk/self").data.url("sidebar.html"),
    onAttach: function (worker) {
      console.log("attaching");
    },
    onShow: function () {
      console.log("showing");
    },
    onHide: function () {
      console.log("hiding");
    },
    onDetach: function () {
      console.log("detaching");
    }
    });

@param options {object}
  Options for the sidebar, with the following keys:
  @prop id {string}
    The `id` of the sidebar. This used to identify this sidebar in its chrome window. It must be unique.
  @prop title {string}
A title for the sidebar. This will be used for the label for your sidebar
in the "Sidebar" submenu in Firefox, and will be shown at the top of your
sidebar when it is open.
  @prop url {string}
    The URL of the content to load in the sidebar. This must be a local URL (typically, loaded from the "data" folder using `self.data.url()`).
  @prop [onAttach] {function}
    Listener for the sidebar's `attach` event.
  @prop [onDetach] {function}
    Listener for the sidebar's `detach` event.
  @prop [onShow] {function}
    Listener for the sidebar's `show` event.
  @prop [onHide] {function}
    Listener for the sidebar's `hide` event.
</api>

<api name="id">
@property {string}
The id of the sidebar. This used to identify this sidebar in its chrome window. It must be unique.
</api>

<api name="title">
@property {string}
The title of the sidebar. This will be used for the label for your sidebar
in the "Sidebar" submenu in Firefox, and will be shown at the top of your
sidebar when it is open.
</api>

<api name="url">
@property {string}
    The URL of the content to load in the sidebar. This must be a local URL (typically, loaded from the "data" folder using `self.data.url()`).
</api>

<api name="dispose">
@method
Destroys the sidebar. Once destroyed, the sidebar can no longer be used.
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
This event is emitted when a worker is attached to a sidebar, as a result of any of the following:

* calling the sidebar's [`show()`](modules/sdk/ui/sidebar.html#show()) method,
when the sidebar is not shown in the currently active window
* changing the sidebar's [`url`](modules/sdk/ui/sidebar.html#url) property
* the user switching the sidebar on using the "Sidebar" submenu in Firefox,
when the sidebar is not shown in the currently active window
* the user opening a new window from a window that has the sidebar showing

This is the event you should listen to if you need to communicate with the
scripts loaded into the sidebar. It is passed a [`worker`](modules/sdk/content/worker.html)
as an argument, which defines `port.emit()` and `port.on()` methods that you can use
to send messages to, and receive messages from, scripts loaded into the sidebar.

This code listens to `attach` and in the listener, starts listening to a message called "ready"
from the script. When the listener receives the "ready" message it sends an "init" message to
the script:

    var sidebar = require("sdk/ui/sidebar").Sidebar({
      id: 'my-sidebar',
      title: 'My Sidebar',
      url: require("sdk/self").data.url("sidebar.html"),
      onAttach: function (worker) {
        // listen for a "ready" message from the script
        worker.port.on("ready", function() {
          // send an "init" message to the script
          worker.port.emit("init", "message from main.js");
        });
      }
    });

Here's the corresponding script, that sends "ready" as soon as it runs, and listens for
the "init" message from main.js:

    addon.port.emit("ready");
    addon.port.on("init", sayStuff);

    function sayStuff(message) {
      console.log("got the message '" + message + "'" );
    }

You should not try to send messages to the script in your `attach` handler, because it's not
guaranteed that the script is listening to them yet. It's safer to start by listening for a
message from the script, as in this example.

</api>

<api name="detach">
@event
This event is emitted when a worker is detached from a sidebar, as a result of either of the following:

* calling the sidebar's [`hide()`](modules/sdk/ui/sidebar.html#hide()) method,
when the sidebar is being shown in the currently active window
* the user switching the sidebar off using the "Sidebar" submenu in Firefox,
when the sidebar is being shown in the currently active window

The `detach` listener receives a [`worker`](modules/sdk/content/worker.html) object as a parameter.
This object is the same as the worker passed into the corresponding `attach` event. After
`detach`, this worker can no longer be used to communicate with the scripts in that sidebar
instance, because it has been unloaded.

If you listen to `attach`, and in the listener take a reference to the worker object that's
passed into it, so you can send it messages later on, then you should probably listen to `detach`,
and in its handler, remove your reference to the worker.

Here's an add-on that adds each worker to an array in the `attach` handler, and makes sure
that its references are cleaned up by listening to `detach` and removing workers as they are
detached:

    var workerArray = [];

    function attachWorker(worker) {
      workerArray.push(worker);
    }

    function detachWorker(worker) {
      var index = workerArray.indexOf(worker);
      if(index != -1) {
        workerArray.splice(index, 1);
      }
    }

    var sidebar = require("sdk/ui/sidebar").Sidebar({
      id: 'my-sidebar',
      title: 'My Sidebar',
      url: require("sdk/self").data.url("sidebar.html"),
      onAttach: attachWorker,
      onDetach: detachWorker
    });

</api>

<api name="show">
@event
This event is emitted when the sidebar is shown, as a result of any of the following:

* calling the sidebar's [`show()`](modules/sdk/ui/sidebar.html#show()) method,
when the sidebar is not shown in the currently active window
* changing the sidebar's [`url`](modules/sdk/ui/sidebar.html#url) property
* the user switching the sidebar on using the "Sidebar" submenu in Firefox,
when the sidebar is not shown in the currently active window
* the user opening a new window from a window that has the sidebar showing

</api>

<api name="hide">
@event
This event is emitted when the sidebar is hidden, as a result of either of the following:

* calling the sidebar's [`hide()`](modules/sdk/ui/sidebar.html#hide()) method,
when the sidebar is being shown in the currently active window
* the user switching the sidebar off using the "Sidebar" submenu in Firefox,
when the sidebar is being shown in the currently active window

</api>

</api>
