<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Felipe Gomes [felipc@gmail.com]  -->

The `windows` module provides basic functions for working with browser
windows. With this module, you can:

* [enumerate the currently opened browser windows](modules/sdk/windows.html#browserWindows)
* [open new browser windows](modules/sdk/windows.html#open(options))
* [listen for common window events such as open and close](modules/sdk/windows.html#Events)

## Private Windows ##

If your add-on has not opted into private browsing, then you won't see any
private browser windows. Private browser windows won't appear in the
[`browserWindows`](modules/sdk/windows.html#browserWindows) property, you
won't receive any window events, and you won't be able to open private 
windows.

To learn more about private windows, how to opt into private browsing, and how
to support private browsing, refer to the
[documentation for the `private-browsing` module](modules/sdk/private-browsing.html).

<api name="browserWindows">
@property {List}
`browserWindows` provides access to all the currently open browser windows as
[BrowserWindow](modules/sdk/windows.html#BrowserWindow) objects.

    var windows = require("sdk/windows");
    for each (var window in windows.browserWindows) {
      console.log(window.title);
    }

    console.log(windows.browserWindows.length);

This object emits all the events listed under the "Events" section:

    var windows = require("sdk/windows").browserWindows;

    // add a listener to the 'open' event
    windows.on('open', function(window) {
      myOpenWindows.push(window);
    });

    // add a listener to the 'close' event
    windows.on('close', function(window) {
      console.log("A window was closed.");
    });

    // add a listener to the 'activate' event
    windows.on('activate', function(window) {
      console.log("A window was activated.");
    });

    // add a listener to the 'deactivate' event
    windows.on('deactivate', function(window) {
      console.log("A window was deactivated.");
    });


<api name="activeWindow">
@property {BrowserWindow}

The currently active window. This property is read-only.

**Example**

    // get
    var windows = require("sdk/windows");
    console.log("title of active window is " +
                windows.browserWindows.activeWindow.title);

    anotherWindow.activate();
    // set
    windows.activeWindow == anotherWindow // true
</api>

</api>

<api name="open">
@function
Open a new window.

    var windows = require("sdk/windows").browserWindows;

    // Open a new window.
    windows.open("http://www.example.com");

    // Open a new window and set a listener for "open" event.
    windows.open({
      url: "http://www.example.com",
      onOpen: function(window) {
        // do stuff like listen for content
        // loading.
      }
    });

Returns the window that was opened:

    var widgets = require("sdk/widget");
    var windows = require("sdk/windows").browserWindows;

    var example = windows.open("http://www.example.com");

    var widget = widgets.Widget({
      id: "close-window",
      label: "Close window",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        example.close();
      }
    });

@param options {object}
An object containing configurable options for how this window will be opened,
as well as a callback for being notified when the window has fully opened.

If the only option being used is `url`, then a bare string URL can be passed to
`open` instead of specifying it as a property of the `options` object.

@prop url {string}
String URL to be opened in the new window.
This is a required property.

@prop isPrivate {boolean}
Boolean which will determine whether the new window should be private or not.
If your add-on does not support private browsing this will have no effect.
See the [private-browsing](modules/sdk/private-browsing.html) documentation for more information.

@prop [onOpen] {function}
A callback function that is called when the window has opened. This does not
mean that the URL content has loaded, only that the window itself is fully
functional and its properties can be accessed. This is an optional property.

@prop [onClose] {function}
A callback function that is called when the window will be called.
This is an optional property.

@prop [onActivate] {function}
A callback function that is called when the window is made active.
This is an optional property.

@prop [onDeactivate] {function}
A callback function that is called when the window is made inactive.
This is an optional property.

@returns {BrowserWindow}
</api>

<api name="BrowserWindow">
@class
A `BrowserWindow` instance represents a single open window. They can be
retrieved from the `browserWindows` property exported by this module.

    var windows = require("sdk/windows").browserWindows;

    //Print how many tabs the current window has
    console.log("The active window has " +
                windows.activeWindow.tabs.length +
                " tabs.");

    // Print the title of all browser windows
    for each (var window in windows) {
      console.log(window.title);
    }

    // close the active window
    windows.activeWindow.close(function() {
      console.log("The active window was closed");
    });

<api name="title">
@property {string}
The current title of the window. Usually the title of the active tab,
plus an app identifier.
This property is read-only.
</api>

<api name="tabs">
@property {TabList}
A live list of tabs in this window. This object has the same interface as the
[`tabs` API](modules/sdk/tabs.html), except it contains only the
tabs in this window, not all tabs in all windows. This property is read-only.
</api>

<api name="isPrivateBrowsing">
@property {boolean}
Returns `true` if the window is in private browsing mode, and `false` otherwise.

<div class="warning">
  This property is deprecated.
  From version 1.14, use the <a href="modules/sdk/private-browsing.html#isPrivate()">private-browsing module's <code>isPrivate()</code></a> function instead.
</div>

</api>

<api name="activate">
@method
Makes window active, which will focus that window and bring it to the
foreground.
</api>

<api name="close">
@method
Close the window.

@param [callback] {function}
A function to be called when the window finishes its closing process.
This is an optional argument.
</api>

</api>

<api name="open">
@event
Event emitted when a new window is open.
This does not mean that the content has loaded, only that the browser window
itself is fully visible to the user.
@argument {Window}
Listeners are passed the `window` object that triggered the event.
</api>

<api name="close">
@event
Event emitted when a window is closed.
@argument {Window}
Listeners are passed the `window` object that triggered the event.
</api>

<api name="activate">
@event
Event emitted when an inactive window is made active.
@argument {Window}
Listeners are passed the `window` object that has become active.
</api>

<api name="deactivate">
@event
Event emitted when the active window is made inactive.

@argument {Window}
Listeners are passed the `window` object that has become inactive.
</api>
