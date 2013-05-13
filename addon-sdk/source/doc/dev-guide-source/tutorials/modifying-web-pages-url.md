<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Modifying Web Pages Based on URL #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

To modify any pages that match a particular pattern
(for example, "http://example.org/") as they are loaded, use the
[`page-mod`](modules/sdk/page-mod.html) module.

To create a page-mod you need to specify two things:

* one or more scripts to run. Because their job is to interact with web
content, these scripts are called *content scripts*.
* one or more patterns to match the URLs for the pages you want to modify

Here's a simple example. The content script is supplied as the `contentScript`
option, and the URL pattern is given as the `include` option:

    // Import the page-mod API
    var pageMod = require("sdk/page-mod");

    // Create a page mod
    // It will run a script whenever a ".org" URL is loaded
    // The script replaces the page contents with a message
    pageMod.PageMod({
      include: "*.org",
      contentScript: 'document.body.innerHTML = ' +
                     ' "<h1>Page matches ruleset</h1>";'
    });

Try it out:

* create a new directory and navigate to it
* run `cfx init`
* open the `lib/main.js` file, and add the code above
* run `cfx run`, then run `cfx run` again
* open [ietf.org](http://www.ietf.org) in the browser window that opens

This is what you should see:

<img  class="image-center" src="static-files/media/screenshots/pagemod-ietf.png"
alt="ietf.org eaten by page-mod" />

## Specifying the Match Pattern ##

The match pattern uses the
[`match-pattern`](modules/sdk/page-mod/match-pattern.html)
syntax. You can pass a single match-pattern string, or an array.

## Keeping the Content Script in a Separate File ##

In the example above we've passed in the content script as a string. Unless
the script is extremely simple, you should instead maintain the script as a
separate file. This makes the code easier to maintain, debug, and review.

To do this, you need to:

* save the script in your add-on's `data` directory
* use the `contentScriptFile` option instead of `contentScript`, and pass
it the URL for the script. The URL can be obtained using `self.data.url()`

For example, if we save the script above under the add-on's `data` directory
in a file called `my-script.js`:

    document.body.innerHTML = "<h1>Page matches ruleset</h1>";

We can load this script by changing the page-mod code like this:

    // Import the page-mod API
    var pageMod = require("sdk/page-mod");
    // Import the self API
    var self = require("sdk/self");

    // Create a page mod
    // It will run a script whenever a ".org" URL is loaded
    // The script replaces the page contents with a message
    pageMod.PageMod({
      include: "*.org",
      contentScriptFile: self.data.url("my-script.js")
    });

## Loading Multiple Content Scripts ##

You can load more than one script, and the scripts can interact
directly with each other. So, for example, you could rewrite
`my-script.js` to use jQuery:

    $("body").html("<h1>Page matches ruleset</h1>");

Then download jQuery to your add-on's `data` directory, and
load the script and jQuery together (making sure to load jQuery
first):

    // Import the page-mod API
    var pageMod = require("sdk/page-mod");
    // Import the self API
    var self = require("sdk/self");

    // Create a page mod
    // It will run a script whenever a ".org" URL is loaded
    // The script replaces the page contents with a message
    pageMod.PageMod({
      include: "*.org",
      contentScriptFile: [self.data.url("jquery-1.7.min.js"),
                          self.data.url("my-script.js")]
    });

You can use both `contentScript` and `contentScriptFile`
in the same page-mod: if you do this, scripts loaded using
`contentScript` are loaded first:

    // Import the page-mod API
    var pageMod = require("sdk/page-mod");
    // Import the self API
    var self = require("sdk/self");

    // Create a page mod
    // It will run a script whenever a ".org" URL is loaded
    // The script replaces the page contents with a message
    pageMod.PageMod({
      include: "*.org",
      contentScriptFile: self.data.url("jquery-1.7.min.js"),
      contentScript: '$("body").html("<h1>Page matches ruleset</h1>");'
    });

Note, though, that you can't load a script from a web site. The script
must be loaded from `data`.

## Communicating With the Content Script ##

Your add-on script and the content script can't directly
access each other's variables or call each other's functions, but they
can send each other messages.

To send a
message from one side to the other, the sender calls `port.emit()` and
the receiver listens using `port.on()`.

* In the content script, `port` is a property of the global `self` object.
* In the add-on script, you need to listen for the `onAttach` event to get
passed a [worker](modules/sdk/content/worker.html#Worker) object that
contains `port`.

Let's rewrite the example above to pass a message from the add-on to
the content script. The message will contain the new content to insert into
the document. The content script now needs to look like this:

    // "self" is a global object in content scripts
    // Listen for a message, and replace the document's
    // contents with the message payload.
    self.port.on("replacePage", function(message) {
      document.body.innerHTML = "<h1>" + message + "</h1>";
    });

In the add-on script, we'll send the content script a message inside `onAttach`:

    // Import the page-mod API
    var pageMod = require("sdk/page-mod");
    // Import the self API
    var self = require("sdk/self");

    // Create a page mod
    // It will run a script whenever a ".org" URL is loaded
    // The script replaces the page contents with a message
    pageMod.PageMod({
      include: "*.org",
      contentScriptFile: self.data.url("my-script.js"),
      // Send the content script a message inside onAttach
      onAttach: function(worker) {
        worker.port.emit("replacePage", "Page matches ruleset");
      }
    });

The `replacePage` message isn't a built-in message: it's a message defined by
the add-on in the `port.emit()` call.

<div class="experimental">

## Injecting CSS ##

**Note that the feature described in this section is experimental
at the moment: we'll very probably continue to support the feature,
but details of the API might need to change.**

Rather than injecting JavaScript into a page, you can inject CSS by
setting the page-mod's `contentStyle` option:

    var pageMod = require("sdk/page-mod").PageMod({
      include: "*",
      contentStyle: "body {" +
                    "  border: 5px solid green;" +
                    "}"
    });

As with `contentScript`, there's a corresponding `contentStyleFile` option
that's given the URL of a CSS file in your "data" directory, and it is
good practice to use this option in preference to `contentStyle` if the
CSS is at all complex:

    var pageMod = require("sdk/page-mod").PageMod({
      include: "*",
      contentStyleFile: require("sdk/self").data.url("my-style.css")
    });

You can't currently use relative URLs in style sheets loaded with
`contentStyle` or `contentStyleFile`. If you do, the files referenced
by the relative URLs will not be found.

To learn more about this, and read about a workaround, see the
[relevant section in the page-mod API documentation](modules/sdk/page-mod.html#Working_with_Relative_URLs_in_CSS_Rules).

</div>

## Learning More ##

To learn more about `page-mod`, see its
[API reference page](modules/sdk/page-mod.html).
In particular, the `PageMod` constructor takes several additional options
to control its behavior:

* By default, content scripts are not attached to any tabs that are
already open when the page-mod is created, and are attached to iframes
as well as top-level documents. To control this behavior use the `attachTo`
option.

* Define read-only values accessible to content scripts using the
`contentScriptOptions` option.

* By default, content scripts are attached after all the content
   (DOM, JS, CSS, images) for the page has been loaded, at the time the
   [window.onload event](https://developer.mozilla.org/en/DOM/window.onload)
   fires. To control this behavior use the `contentScriptWhen` option.

To learn more about content scripts in general, see the
[content scripts guide](dev-guide/guides/content-scripts/index.html).
