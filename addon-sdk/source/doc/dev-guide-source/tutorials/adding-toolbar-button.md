<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Adding a Button to the Toolbar #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

To add a button to the toolbar, use the
[`widget`](modules/sdk/widget.html) module.

Create a new directory, navigate to it, and execute `cfx init`.
Then open the file called "main.js" in the "lib" directory and
add the following code to it:

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");

    var widget = widgets.Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        tabs.open("http://www.mozilla.org/");
      }
    });

The widget is added to the "Add-on Bar" at the bottom of the browser window:

<img class="image-right" src="static-files/media/screenshots/widget-mozilla.png"
alt="Mozilla icon widget" />

You can't change the initial location for the widget, but the user can move
it to a different toolbar. The `id` attribute is mandatory, and is used to
remember the position of the widget, so you should not change it in subsequent
versions of the add-on.

Clicking the button opens [http://www.mozilla.org](http://www.mozilla.org).

<div style="clear:both"></div>

## Specifying the Icon ##

If you're using the widget to make a toolbar button, specify the icon to
display using `contentURL`: this may refer to a remote file as in the
example above, or may refer to a local file. The example below will load
an icon file called "my-icon.png" from the add-on's `data` directory:

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");
    var self = require("sdk/self");

    var widget = widgets.Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: self.data.url("my-icon.png"),
      onClick: function() {
        tabs.open("http://www.mozilla.org/");
      }
    });

You can change the icon at any time by setting the widget's `contentURL`
property. However, setting the `contentURL` property will break the
channel of communication between this widget and any content scripts it
contains. Messages sent from the content script will no longer be received
by the main add-on code, and vice versa. This issue is currently tracked as
[bug 825434](https://bugzilla.mozilla.org/show_bug.cgi?id=825434).

## Responding To the User ##

You can listen for `click`, `mouseover`, and `mouseout` events by passing
handler functions as the corresponding constructor options. The widget
example above assigns a listener to the `click` event using the `onClick`
option, and there are similar `onMouseover` and `onMouseout` options.

To handle user interaction in more detail, you can attach a content
script to the widget. Your add-on script and the content script can't
directly access each other's variables or call each other's functions, but
they can send each other messages.

Here's an example. The widget's built-in `onClick` property does not
distinguish between left and right mouse clicks, so to do this we need
to use a content script. The script looks like this:

    window.addEventListener('click', function(event) {
      if(event.button == 0 && event.shiftKey == false)
        self.port.emit('left-click');

      if(event.button == 2 || (event.button == 0 && event.shiftKey == true))
        self.port.emit('right-click');
        event.preventDefault();
    }, true);

It uses the standard DOM `addEventListener()` function to listen for click
events, and handles them by sending the corresponding message to the main
add-on code. Note that the messages "left-click" and "right-click" are not
defined in the widget API itself, they're custom events defined by the add-on
author.

Save this script in your `data` directory as "click-listener.js".

Next, modify `main.js` to:

<ul>
<li>pass in the script by setting the <code>contentScriptFile</code>
property</li>
<li>listen for the new events:</li>
</ul>

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");
    var self = require("sdk/self");

    var widget = widgets.Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      contentScriptFile: self.data.url("click-listener.js")
    });

    widget.port.on("left-click", function(){
      console.log("left-click");
    });

    widget.port.on("right-click", function(){
      console.log("right-click");
    });

Now execute `cfx run` again, and try right- and left-clicking on the button.
You should see the corresponding string written to the command shell.

## Attaching a Panel ##

<!-- The icon the widget displays, shown in the screenshot, is taken from the
Circular icon set, http://prothemedesign.com/circular-icons/ which is made
available under the Creative Commons Attribution 2.5 Generic License:	
http://creativecommons.org/licenses/by/2.5/ -->

<img class="image-right" src="static-files/media/screenshots/widget-panel-clock.png"
alt="Panel attached to a widget">

If you supply a `panel` object to the widget's constructor, then the panel
will be shown when the user clicks the widget:

    data = require("sdk/self").data

    var clockPanel = require("sdk/panel").Panel({
      width:215,
      height:160,
      contentURL: data.url("clock.html")
    });

    require("sdk/widget").Widget({
      id: "open-clock-btn",
      label: "Clock",
      contentURL: data.url("History.png"),
      panel: clockPanel
    });

To learn more about working with panels, see the tutorial on
[displaying a popup](dev-guide/tutorials/display-a-popup.html).

## Learning More ##

To learn more about the widget module, see its
[API reference documentation](modules/sdk/widget.html).

To learn more about content scripts, see the
[content scripts guide](dev-guide/guides/content-scripts/index.html).
