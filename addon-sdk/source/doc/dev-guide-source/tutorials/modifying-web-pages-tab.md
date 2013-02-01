<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Modifying the Page Hosted by a Tab #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

To modify the page hosted by a particular tab, load a script into it
using the `attach()` method of the
[tab](modules/sdk/tabs.html) object. Because their job is
to interact with web content, these scripts are called *content scripts*.

Here's a simple example:

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");

    var widget = widgets.Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        tabs.activeTab.attach({
          contentScript:
            'document.body.style.border = "5px solid red";'
          })
        }
    });

This add-on creates a widget which contains the Mozilla favicon as an icon.
It has a click handler which fetches the active tab and loads a
script into the page hosted by the active tab. The script is specified using
`contentScript` option, and just draws
a red border around the page. Try it out:

* create a new directory and navigate to it
* run `cfx init`
* open the `lib/main.js` file, and add the code above
* run `cfx run`, then run `cfx run` again

You should see the Mozilla icon appear in the bottom-right corner of the
browser:

<img class="image-center" src="static-files/media/screenshots/widget-mozilla.png"
alt="Mozilla icon widget" />

Then open any web page in the browser window that opens, and click the
Mozilla icon. You should see a red border appear around the page, like this:

<img class="image-center" src="static-files/media/screenshots/tabattach-bbc.png"
alt="bbc.co.uk modded by tab.attach" />

## Keeping the Content Script in a Separate File ##

In the example above we've passed in the content script as a string. Unless
the script is extremely simple, you should instead maintain the script as a
separate file. This makes the code easier to maintain, debug, and review.

For example, if we save the script above under the add-on's `data` directory
in a file called `my-script.js`:

    document.body.style.border = "5px solid red";

We can load this script by changing the add-on code like this:

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");
    var self = require("sdk/self");

    var widget = widgets.Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        tabs.activeTab.attach({
          contentScriptFile: self.data.url("my-script.js")
        });
      }
    });

You can load more than one script, and the scripts can interact
directly with each other. So you can load [jQuery](http://jquery.com/),
and then your content script can use that.

## Communicating With the Content Script ##

Your add-on script and the content script can't directly
access each other's variables or call each other's functions, but they
can send each other messages.

To send a
message from one side to the other, the sender calls `port.emit()` and
the receiver listens using `port.on()`.

* In the content script, `port` is a property of the global `self` object.
* In the add-on script, `tab-attach()` returns an object containing the
`port` property you use to send messages to the content script.

Let's rewrite the example above to pass a message from the add-on to
the content script. The content script now needs to look like this:

    // "self" is a global object in content scripts
    // Listen for a "drawBorder"
    self.port.on("drawBorder", function(color) {
      document.body.style.border = "5px solid " + color;
    });

In the add-on script, we'll send the content script a "drawBorder" message
using the object returned from `attach()`:

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");
    var self = require("sdk/self");

    var widget = widgets.Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        worker = tabs.activeTab.attach({
          contentScriptFile: self.data.url("my-script.js")
        });
        worker.port.emit("drawBorder", "red");
      }
    });

The `drawBorder` message isn't a built-in message, it's one that this
add-on defines in the `port.emit()` call.

## Injecting CSS ##

Unlike the [`page-mod`](dev-guide/tutorials/modifying-web-pages-url.html) API,
`tab.attach()` doesn't enable you to inject CSS directly into a page.

To modify the style of a page you have to use JavaScript, as in
the example above.

## Learning More ##

To learn more about working with tabs in the SDK, see the
[Open a Web Page](dev-guide/tutorials/open-a-web-page.html)
tutorial, the
[List Open Tabs](dev-guide/tutorials/list-open-tabs.html)
tutorial, and the [`tabs` API reference](modules/sdk/tabs.html).

To learn more about content scripts, see the
[content scripts guide](dev-guide/guides/content-scripts/index.html).