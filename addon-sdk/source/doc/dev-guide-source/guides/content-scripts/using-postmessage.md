<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Communicating using "postMessage()" #

As an alternative to user-defined events content modules support the built-in
`message` event. In most cases user-defined events are preferable to message
events. However, the `context-menu` module does not support user-defined
events, so to send messages from a content script to the add-on via a context
menu object, you must use message events.

## Handling Message Events in the Content Script ##

To send a message from a content script, you use the `postMessage` function of
the global `self` object:

    self.postMessage(contentScriptMessage);

This takes a single parameter, the message payload, which may be any
<a href = "dev-guide/guides/content-scripts/using-port.html#json_serializable">JSON-serializable value</a>.

To receive a message from the add-on script, use `self`'s `on` function:

    self.on("message", function(addonMessage) {
      // Handle the message
    });

Like all event-registration functions, this takes two parameters: the name
of the event, and the handler function. The handler function is passed the
message payload.

## Handling Message Events in the Add-on Script ##

To send a message to a content script, use the worker's `postMessage`
function. Again, `panel` and `page` integrate `worker` directly:

    // Post a message to the panel's content scripts
    panel.postMessage(addonMessage);

However, for `page-mod` objects you need to listen to the `onAttach` event
and use the worker supplied to that:

    var pageMod = require('sdk/page-mod').PageMod({
      include: ['*'],
      contentScript: pageModScript,
      onAttach: function(worker) {
        worker.postMessage(addonMessage);
      }
    });

To receive messages from a content script, use the worker's `on` function.
To simplify this most content modules provide an `onMessage` property as an
argument to the constructor:

    panel = require("sdk/panel").Panel({
      onMessage: function(contentScriptMessage) {
        // Handle message from the content script
      }
    });

## Timing Issues Using postMessage ##

Content scripts are loaded according to the value of the
[`contentScriptWhen`](dev-guide/guides/content-scripts/loading.html)
option: until that point is reached, any attempt to send a message to
the script using `postMessage()` will trigger an exception, probably
this:

<span class="aside">
This is a generic message which is emitted whenever we try to
send a message to a content script, but can't find the worker
which is supposed to receive it.
</span>

<pre>
Error: Couldn't find the worker to receive this message. The script may not be initialized yet, or may already have been unloaded.
</pre>

So code like this, where we create a panel and then
synchronously send it a message using `postMessage()`, will not work:

    var data = require("sdk/self").data;

    var panel = require("sdk/panel").Panel({
      contentURL: "http://www.bbc.co.uk/mobile/index.html",
      contentScriptFile: data.url("panel.js")
    });

    panel.postMessage("hi from main.js");

[`port.emit()`](dev-guide/guides/content-scripts/using-port.html)
queues messages until the content script is ready to receive them,
so the equivalent code using `port.emit()` will work:

    var data = require("sdk/self").data;

    var panel = require("sdk/panel").Panel({
      contentURL: "http://www.bbc.co.uk/mobile/index.html",
      contentScriptFile: data.url("panel.js")
    });

    panel.port.emit("hi from main.js");


## Message Events Versus User-Defined Events ##

You can use message events as an alternative to user-defined events:

    var pageModScript = "window.addEventListener('mouseover', function(event) {" +
                        "  self.postMessage(event.target.toString());" +
                        "}, false);";

    var pageMod = require('sdk/page-mod').PageMod({
      include: ['*'],
      contentScript: pageModScript,
      onAttach: function(worker) {
        worker.on('message', function(message) {
          console.log('mouseover: ' + message);
        });
      }
    });

The reason to prefer user-defined events is that as soon as you need to send
more than one type of message, then both sending and receiving messages gets
more complex.

Suppose the content script wants to send `mouseout` events as well as
`mouseover`. Now we have to embed the event type in the message payload, and
implement a switch function in the receiver to dispatch the message:

    var pageModScript = "window.addEventListener('mouseover', function(event) {" +
                        "  self.postMessage({" +
                        "    kind: 'mouseover'," +
                        "    element: event.target.toString()" +
                        "  });" +
                        "}, false);" +
                        "window.addEventListener('mouseout', function(event) {" +
                        "  self.postMessage({" +
                        "    kind: 'mouseout'," +
                        "    element: event.target.toString()" +
                        "  });" +
                        " }, false);"


    var pageMod = require('sdk/page-mod').PageMod({
      include: ['*'],
      contentScript: pageModScript,
      onAttach: function(worker) {
        worker.on('message', function(message) {
        switch(message.kind) {
          case 'mouseover':
            console.log('mouseover: ' + message.element);
            break;
          case 'mouseout':
            console.log('mouseout: ' + message.element);
            break;
          }
        });
      }
    });

Implementing the same add-on with user-defined events is shorter and more
readable:

    var pageModScript = "window.addEventListener('mouseover', function(event) {" +
                        "  self.port.emit('mouseover', event.target.toString());" +
                        "}, false);" +
                        "window.addEventListener('mouseout', function(event) {" +
                        "  self.port.emit('mouseout', event.target.toString());" +
                        "}, false);";

    var pageMod = require('sdk/page-mod').PageMod({
      include: ['*'],
      contentScript: pageModScript,
      onAttach: function(worker) {
        worker.port.on('mouseover', function(message) {
          console.log('mouseover :' + message);
        });
        worker.port.on('mouseout', function(message) {
          console.log('mouseout :' + message);
        });
      }
    });
