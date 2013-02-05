<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->


# Communicating using "port" #

To enable add-on scripts and content scripts to communicate with each other,
each end of the conversation has access to a `port` object which defines two
functions:

**`emit()`** is used to emit an event. It may be called with any number of
parameters, but is most likely to be called with a name for the event and
an optional payload. The payload can be any value that is
<a href = "dev-guide/guides/content-scripts/using-port.html#json_serializable">serializable to JSON</a>

    port.emit("myEvent", myEventPayload);

**`on()`** takes two parameters: the name of the event and a function to handle it:

    port.on("myEvent", function handleMyEvent(myEventPayload) {
      // Handle the event
    });

Here's  simple add-on that sends a message to a content script using `port`:

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

We could depict the interface between add-on code and content script code like
this:

<img class="image-center" src="static-files/media/content-scripting-events.png"
alt="Content script events">

Events are asynchronous: that is, the sender does not wait for a reply from
the recipient but just emits the event and continues processing.

## Accessing `port` in the Content Script ##

<span class="aside">Note that the global `self` object is completely
different from the [`self` module](modules/sdk/self.html), which
provides an API for an add-on to access its data files and ID.</span>

In the content script the `port` object is available as a property of the
global `self` object. Thus, to emit an event from a content script:

    self.port.emit("myContentScriptEvent", myContentScriptEventPayload);

To receive an event from the add-on code:

    self.port.on("myAddonEvent", function(myAddonEventPayload) {
      // Handle the event
    });

Compare this to the technique used to receive _built-in_ events in the
content script. For example, to receive the `context` event in a content script
associated with a [context menu](modules/sdk/context-menu.html)
object, you would call the `on` function attached to the global `self` object:

    self.on("context", function() {
      // Handle the event
    });

So the `port` property is essentially used here as a namespace for
user-defined events.

## Accessing `port` in the Add-on Script ##

In the add-on code, the channel of communication between the add-on and a
particular content script context is encapsulated by the `worker` object. Thus
the `port` object for communicating with a content script is a property of the
corresponding `worker` object.

However, the worker is not exposed to add-on code in quite the same way
in all modules. The `panel` and `page-worker` objects integrate the
worker API directly. So to receive events from a content script associated
with a panel you use `panel.port.on()`:

    var panel = require("sdk/panel").Panel({
      contentScript: "self.port.emit('showing', 'panel is showing');"
    });

    panel.port.on("showing", function(text) {
      console.log(text);
    });

    panel.show();

Conversely, to emit user-defined events from your add-on you can just call
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
content script is attached to a page, the worker associated with the page is
supplied to the page-mod in its `onAttach` function. By supplying a target for
this function in the page-mod's constructor you can register to receive
events from the content script, and take a reference to the worker so as to
emit events to it.

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

In the add-on above there are two user-defined events:

* `click` is sent from the page-mod to the add-on, when the user clicks an
element in the page
* `warning` sends a silly string back to the page-mod

## <a name="json_serializable">JSON-Serializable Values</a> ##

The payload for an event can be any JSON-serializable value. When events are
sent their payloads are automatically serialized, and when events are received
their payloads are automatically deserialized, so you don't need to worry
about serialization.

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
