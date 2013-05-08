<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Communicating With Other Scripts #

This section of the guide explains how content scripts can
communicate with:

* [your `main.js` file](dev-guide/guides/content-scripts/communicating-with-other-scripts.html#main.js),
or any other modules in your add-on
* [other content scripts loaded by your add-on](dev-guide/guides/content-scripts/communicating-with-other-scripts.html#Content Scripts)
* [page scripts](dev-guide/guides/content-scripts/communicating-with-other-scripts.html#Page Scripts) (that is, scripts embedded in the web page or
included using `<script>` tags) 

## main.js ##

Your content scripts can communicate with your add-on's "main.js"
(or any other modules you're written for your add-on) by sending it messages,
using either the `port.emit()` API or the `postMessage()` API. See the
articles on
[using `postMessage()`](dev-guide/guides/content-scripts/using-postmessage.html)
and
[using `port`](dev-guide/guides/content-scripts/using-port.html) for details.

## Content Scripts ##

Content scripts loaded into the same document can interact
with each other directly as well as with the web content itself. However,
content scripts which have been loaded into different documents
cannot interact with each other.

For example:

* if an add-on creates a single `panel` object and loads several content
scripts into the panel, then they can interact with each other
* if an add-on creates two `panel` objects and loads a script into each
one, they can't interact with each other.
* if an add-on creates a single `page-mod` object and loads several content
scripts into the page mod, then only content scripts associated with the
same page can interact with each other: if two different matching pages are
loaded, content scripts attached to page A cannot interact with those attached
to page B.

The web content has no access to objects created by the content script, unless
the content script explicitly makes them available.

## Page Scripts ##

If a page includes its own scripts using `<script>` tags,
either embedded in the page or linked to it using the `src` attribute, there
are a couple of ways a content script can communicate with it:

* using the [DOM `postMessage()` API](dev-guide/guides/content-scripts/communicating-with-other-scripts.html#Using the DOM postMessage API)
* using [custom DOM events](dev-guide/guides/content-scripts/communicating-with-other-scripts.html#Using Custom DOM Events)

### Using the DOM postMessage API ###

You can communicate between the content script and page scripts using
[`window.postMessage()`](https://developer.mozilla.org/en/DOM/window.postMessage),
but there's a twist: in early versions of the SDK, the global `postMessage()`
function in content scripts was used for communicating between the content
script and the main add-on code. Although this has been
[deprecated in favor of `self.postMessage`](https://wiki.mozilla.org/Labs/Jetpack/Release_Notes/1.0b5#Major_Changes),
the old globals are still supported, so you can't currently use
`window.postMessage()`. You must use `document.defaultView.postMessage()`
instead.

#### Messaging From Content Script To Page Script ####

Suppose we have a page called "listen.html" hosted at "my-domain.org", and we want to send messages
from the add-on to a script embedded in that page.

In the main add-on code, we have a
[`page-mod`](modules/sdk/page-mod.html) that attaches the content script
"talk.js" to the right page:

    var data = require("sdk/self").data;

    var pageMod = require("sdk/page-mod");
    pageMod.PageMod({
      include: "http://my-domain.org/listen.html",
      contentScriptFile: data.url("talk.js")
    });

The "talk.js" content script uses `document.defaultView.postMessage()` to send
the message to the page:

    document.defaultView.postMessage("Message from content script", "http://my-domain.org/");

The second argument may be '*' which will allow communication with any domain.

Finally, "listen.html" uses `window.addEventListener()` to listen for
messages from the content script:

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
&lt;!DOCTYPE html&gt;
&lt;html&gt;
  &lt;head&gt;&lt;/head&gt;
  &lt;body&gt;
    &lt;script&gt;
      window.addEventListener('message', function(event) {
        window.alert(event.data);
      }, false);
    &lt;/script&gt;
  &lt;/body&gt;

&lt;/html&gt;
]]>
</script>

#### Messaging From Page Script To Content Script ####

Sending messages from the page script to the content script is just
the same, but in reverse.

Here "main.js" creates a [`page-mod`](modules/sdk/page-mod.html)
that attaches "listen.js" to the web page:

    var data = require("sdk/self").data;

    var pageMod = require("sdk/page-mod");
    pageMod.PageMod({
      include: "http://my-domain.org/talk.html",
      contentScriptFile: data.url("listen.js")
    });

The web page "talk.html" embeds a script that uses `window.postMessage()`
to send the content script a message when the user clicks a button:

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
&lt;!DOCTYPE html&gt;
&lt;html&gt;
  &lt;head&gt;&lt;/head&gt;
  &lt;body&gt;
    &lt;script&gt;
      function sendMessage() {
        window.postMessage("Message from page script", "http://my-domain.org/");
      }
    &lt;/script&gt;
    &lt;button onclick="sendMessage()"&gt;Send Message&lt;/button&gt;
  &lt;/body&gt;

&lt;/html&gt;
</script>

Finally, the content script "listen.js" uses
`document.defaultView.addEventListener()` to listen for messages from the page
script:

    document.defaultView.addEventListener('message', function(event) {
      console.log(event.data);
      console.log(event.origin);
    }, false);

### Using Custom DOM Events ###

As an alternative to using `postMessage()` you can use
[custom DOM events](https://developer.mozilla.org/en/DOM/CustomEvent)
to communicate between page scripts and content scripts.

#### Messaging From Content Script To Page Script ####

Here's an example showing how to use custom DOM events to send a message
from a content script to a page script.

First, "main.js" will create a [`page-mod`](modules/sdk/page-mod.html)
that will attach "talk.js" to the target web page:

    var data = require("sdk/self").data;

    var pageMod = require("sdk/page-mod");
    pageMod.PageMod({
      include: "http://my-domain.org/listen.html",
      contentScriptFile: data.url("talk.js")
    });

Next, "talk.js" creates and dispatches a custom event, passing the payload
in the `detail` parameter to `initCustomEvent()`:

<!-- This comment is used to terminate the Markdown list above -->

    var event = document.createEvent('CustomEvent');
    event.initCustomEvent("addon-message", true, true, { hello: 'world' });
    document.documentElement.dispatchEvent(event);

Finally "listen.html" listens for the new event and examines its
`detail` attribute to retrieve the payload:

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
&lt;!DOCTYPE html&gt;
&lt;html&gt;
  &lt;head&gt;&lt;/head&gt;
  &lt;body&gt;
    &lt;script&gt;
      document.documentElement.addEventListener("addon-message", function(event) {
        window.alert(JSON.stringify(event.detail))
      }, false);
    &lt;/script&gt;
  &lt;/body&gt;
&lt;/html&gt;
</script>

#### Messaging From Page Script to Content Script ####

Sending messages using custom DOM events from the page script
to the content script is just the same, but in reverse.

Again, "main.js" creates a [`page-mod`](modules/sdk/page-mod.html)
to target the page we are interested in:

    var data = require("sdk/self").data;

    var pageMod = require("sdk/page-mod");
    pageMod.PageMod({
      include: "http://my-domain.org/talk.html",
      contentScriptFile: data.url("listen.js")
    });

The web page "talk.html" creates and dispatches a custom DOM event,
using `initCustomEvent()`'s `detail` parameter to supply the payload:

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
&lt;!DOCTYPE html&gt;
&lt;html&gt;
  &lt;head&gt;&lt;/head&gt;
  &lt;body&gt;
    &lt;script&gt;
      function sendMessage() {
        var event = document.createEvent('CustomEvent');
        event.initCustomEvent("addon-message", true, true, { hello: 'world' });
        document.documentElement.dispatchEvent(event);
      }
    &lt;/script&gt;
    &lt;button onclick="sendMessage()"&gt;Send Message&lt;/button&gt;
  &lt;/body&gt;
&lt;/html&gt;
</script>

Finally, the content script "listen.js" listens for the new event
and retrieves the payload from its `detail` attribute:

    document.documentElement.addEventListener("addon-message", function(event) {
      console.log(JSON.stringify(event.detail));
    }, false);

