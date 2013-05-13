<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->


# Communicating using "port" #

To enable add-on scripts and content scripts to communicate with each other,
each end of the conversation has access to a `port` object.

* to send messages from one side to the other, use `port.emit()`
* to receive messages sent from the other side, use `port.on()`

<img class="image-center" src="static-files/media/content-scripting-events.png"
alt="Content script events">

Messages are asynchronous: that is, the sender does not wait for a reply from
the recipient but just emits the message and continues processing.

Here's a simple add-on that sends a message to a content script using `port`:

    var tabs = require("sdk/tabs");

    var alertContentScript = "self.port.on('alert', function(message) {" +
                             "  window.alert(message);" +
                             "})";

    tabs.on("ready", function(tab) {
      worker = tab.attach({
        contentScript: alertContentScript
      });
      worker.port.emit("alert", "Message from the add-on");
    });

    tabs.open("http://www.mozilla.org");

In total, the `port` object defines four functions:

* [`emit()`](dev-guide/guides/content-scripts/using-port.html#port.emit()):
emit a message.
* [`on()`](dev-guide/guides/content-scripts/using-port.html#port.on()):
listen to a message.
* [`removeListener()`](dev-guide/guides/content-scripts/using-port.html#port.removeListener()):
stop listening to a message.
* [`once()`](dev-guide/guides/content-scripts/using-port.html#port.once()):
listen to only the first occurrence of a message.

## Accessing `port` ##

### Accessing `port` in the Content Script ###

<span class="aside">Note that the global `self` object is completely
different from the [`self` module](modules/sdk/self.html), which
provides an API for an add-on to access its data files and ID.</span>

In the content script the `port` object is available as a property of the
global `self` object. Thus, to emit a message from a content script:

    self.port.emit("myContentScriptMessage", myContentScriptMessagePayload);

To receive a message from the add-on code:

    self.port.on("myAddonMessage", function(myAddonMessagePayload) {
      // Handle the message
    });

Compare this to the technique used to receive _built-in_ messages in the
content script. For example, to receive the `context` message in a content script
associated with a [context menu](modules/sdk/context-menu.html)
object, you would call the `on` function attached to the global `self` object:

    self.on("context", function() {
      // Handle the message
    });

So the `port` property is essentially used here as a namespace for
user-defined messages.

### Accessing `port` in the Add-on Script ###

In the add-on code, the channel of communication between the add-on and a
particular content script context is encapsulated by the 
[`worker`](modules/sdk/content/worker.html#Worker) object. Thus
the `port` object for communicating with a content script is a property of the
corresponding `worker` object.

However, the worker is not exposed to add-on code in quite the same way
in all modules. The `panel` and `page-worker` objects integrate the
worker API directly. So to receive messages from a content script associated
with a panel you use `panel.port.on()`:

    var panel = require("sdk/panel").Panel({
      contentScript: "self.port.emit('showing', 'panel is showing');"
    });

    panel.port.on("showing", function(text) {
      console.log(text);
    });

    panel.show();

Conversely, to emit user-defined messages from your add-on you can just call
`panel.port.emit()`:

    var panel = require("sdk/panel").Panel({
      contentScript: "self.port.on('alert', function(text) {" +
                     "  console.log(text);" +
                     "});"
    });

    panel.show();
    panel.port.emit("alert", "panel is showing");

The `panel` and `page-worker` objects only host a single page at a time,
so each distinct page object only needs a single channel of communication
to its content scripts. But some modules, such as `page-mod`, might need to
handle multiple pages, each with its own context in which the content scripts
are executing, so it needs a separate channel (worker) for each page.

So `page-mod` does not integrate the worker API directly: instead, each time a
content script is attached to a page, the 
[worker](modules/sdk/content/worker.html#Worker) associated with the page is
supplied to the page-mod in its `onAttach` function. By supplying a target for
this function in the page-mod's constructor you can register to receive
messages from the content script, and take a reference to the worker so as to
emit messages to the content script.

    var pageModScript = "window.addEventListener('click', function(event) {" +
                        "  self.port.emit('click', event.target.toString());" +
                        "  event.stopPropagation();" +
                        "  event.preventDefault();" +
                        "}, false);" +
                        "self.port.on('warning', function(message) {" +
                        "window.alert(message);" +
                        "});"

    var pageMod = require('sdk/page-mod').PageMod({
      include: ['*'],
      contentScript: pageModScript,
      onAttach: function(worker) {
        worker.port.on('click', function(html) {
          worker.port.emit('warning', 'Do not click this again');
        });
      }
    });

In the add-on above there are two user-defined messages:

* `click` is sent from the page-mod to the add-on, when the user clicks an
element in the page
* `warning` sends a silly string back to the page-mod

## port.emit() ##

The `port.emit()` function sends a message from the "main.js", or another
add-on module, to a content script, or vice versa.

It may be called with any number of parameters, but is most likely to be
called with a name for the message and an optional payload.
The payload can be any value that is
<a href = "dev-guide/guides/content-scripts/using-port.html#json_serializable">serializable to JSON</a>.

From the content script to the main add-on code:

    var myMessagePayload = "some data";
    self.port.emit("myMessage", myMessagePayload);

From the main add-on code (in this case a panel instance)
to the content script:

    var myMessagePayload = "some data";
    panel.port.emit("myMessage", myMessagePayload);

## port.on() ##

The `port.on()` function registers a function as a listener for a specific
named message sent from the other side using `port.emit()`.

It takes two parameters: the name of the message and a function to handle it.

In a content script, to listen for "myMessage" sent from the main
add-on code:

    self.port.on("myMessage", function handleMyMessage(myMessagePayload) {
      // Handle the message
    });

In the main add-on code (in this case a panel instance), to listen for
"myMessage" sent from a a content script:

    panel.port.on("myMessage", function handleMyMessage(myMessagePayload) {
      // Handle the message
    });

## port.removeListener() ##

You can uses `port.on()` to listen for messages. To stop listening for a
particular message, use `port.removeListener()`. This takes the
same two parameters as `port.on()`: the name of the message, and the name
of the listener function.

For example, here's an add-on that creates a page-worker and a widget.
The page-worker loads
[http://en.wikipedia.org/wiki/Chalk](http://en.wikipedia.org/wiki/Chalk)
alongside a content script "listener.js". The widget sends the content script
a message called "get-first-para" when it is clicked:

    pageWorker = require("sdk/page-worker").Page({
      contentScriptFile: require("sdk/self").data.url("listener.js"),
      contentURL: "http://en.wikipedia.org/wiki/Chalk"
    });

    require("sdk/widget").Widget({
      id: "mozilla-icon",
      label: "My Mozilla Widget",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        console.log("sending 'get-first-para'");
        pageWorker.port.emit("get-first-para");
      }
    });

The content script "listener.js" listens for "get-first-para". When it
receives this message, the script logs the first paragraph of the document
and then calls `removeListener()` to stop listening.

    function getFirstParagraph() {
      var paras = document.getElementsByTagName('p');
      console.log(paras[0].textContent);
      self.port.removeListener("get-first-para", getFirstParagraph);
    }

    self.port.on("get-first-para", getFirstParagraph);

The result is that the paragraph is only logged the first time the widget
is clicked.

Due to [bug 816272](https://bugzilla.mozilla.org/show_bug.cgi?id=816272)
the [`page-mod`](modules/sdk/page-mod.html)'s `removeListener()` function
does not prevent the listener from receiving messages that are already queued.
This means that if "main.js" sends the message twice in successive lines, and
the listener stops listening as soon as it receives the first message, then
the listener will still receive the second message.

## port.once() ##

Often you'll want to receive a message just once, then stop listening. The
`port` object offers a shortcut to do this: the `once()` method.

This example rewrites the "listener.js" content script in the
[`port.removeListener()` example](dev-guide/guides/content-scripts/using-port.html#port.removeListener())
so that it uses `once()`:

    function getFirstParagraph() {
      var paras = document.getElementsByTagName('p');
      console.log(paras[0].textContent);
    }

    self.port.once("get-first-para", getFirstParagraph);

## <a name="json_serializable">JSON-Serializable Values</a> ##

The payload for an message can be any JSON-serializable value. When messages
are sent their payloads are automatically serialized, and when messages are
received their payloads are automatically deserialized, so you don't need to
worry about serialization.

However, you _do_ have to ensure that the payload can be serialized to JSON.
This means that it needs to be a string, number, boolean, null, array of
JSON-serializable values, or an object whose property values are themselves
JSON-serializable. This means you can't send functions, and if the object
contains methods they won't be encoded.

For example, to include an array of strings in the payload:

    var pageModScript = "self.port.emit('loaded'," +
                        "  [" +
                        "  document.location.toString()," +
                        "  document.title" +
                        "  ]" +
                        ");"

    var pageMod = require('page-mod').PageMod({
      include: ['*'],
      contentScript: pageModScript,
      onAttach: function(worker) {
        worker.port.on('loaded', function(pageInfo) {
          console.log(pageInfo[0]);
          console.log(pageInfo[1]);
        });
      }
    });
