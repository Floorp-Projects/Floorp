<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Creating Event Targets #

<span class="aside">This tutorial describes the use of low-level APIs.
These APIs are still in active development, and we expect to make
incompatible changes to them in future releases.</span>

The [guide to event-driven programming with the SDK](dev-guide/guides/events.html)
describes how to consume events: that is, how to listen to events generated
by event targets. For example, you can listen to the [`tabs` module's `ready` event](modules/sdk/tabs.html#ready) or the
[`Panel` object's `show` event](modules/sdk/panel.html#show).

With the SDK, it's also simple to implement your own event targets.
This is especially useful if you want to
[build your own modules](dev-guide/tutorials/reusable-modules.html),
either to organize your add-on better or to enable other developers to
reuse your code. If you use the SDK's event framework for your
event targets, users of your module can listen for events using the SDK's
standard event API.

In this tutorial we'll create part of a module to access the browser's
[Places API](https://developer.mozilla.org/en/Places). It will emit events
when the user adds and visits bookmarks, enabling users of the module
to listen for these events using the SDK's standard event API.

## Using the Places API ##

First, let's write some code using Places API that logs the URIs of
bookmarks the user adds.

Create a new directory called "bookmarks", navigate to it, and run `cfx init`.
Then open "lib/main.js" and add the following code:

    var {Cc, Ci, Cu} = require("chrome");
    Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
    var bookmarkService = Cc["@mozilla.org/browser/nav-bookmarks-service;1"]
                              .getService(Ci.nsINavBookmarksService);

    var bookmarkObserver = {
      onItemAdded: function(aItemId, aFolder, aIndex) {
        console.log("added ", bookmarkService.getBookmarkURI(aItemId).spec);
      },
      onItemVisited: function(aItemId, aVisitID, time) {
        console.log("visited ", bookmarkService.getBookmarkURI(aItemId).spec);
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
    };

    exports.main = function() {
      bookmarkService.addObserver(bookmarkObserver, false);    
    };

    exports.onUnload = function() {
      bookmarkService.removeObserver(bookmarkObserver);
    }

Try running this add-on, adding and visiting bookmarks, and observing
the output in the console.

## Modules as Event Targets ##

We can adapt this code into a separate module that exposes the SDK's
standard event interface.

To do this we'll use the [`event/core`](modules/sdk/event/core.html)
module.

Create a new file in "lib" called "bookmarks.js", and add the following code:

    var { emit, on, once, off } = require("sdk/event/core");

    var {Cc, Ci, Cu} = require("chrome");
    Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
    var bookmarkService = Cc["@mozilla.org/browser/nav-bookmarks-service;1"]
                              .getService(Ci.nsINavBookmarksService);

    var bookmarkObserver = {
      onItemAdded: function(aItemId, aFolder, aIndex) {
        emit(exports, "added", bookmarkService.getBookmarkURI(aItemId).spec);
      },
      onItemVisited: function(aItemId, aVisitID, time) {
        emit(exports, "visited", bookmarkService.getBookmarkURI(aItemId).spec);
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
    };

    bookmarkService.addObserver(bookmarkObserver, false);

    exports.on = on.bind(null, exports);
    exports.once = once.bind(null, exports);
    exports.removeListener = function removeListener(type, listener) {
      off(exports, type, listener);
    };

This code implements a module which can emit `added` and `visited` events.
It duplicates the previous code, but with a few changes:

* import `emit()`, `on()`, `once()`, and `off()` from `event/core`
* replace listener functions with calls to `emit()`, passing the appropriate
event type
* export its own event API. This consists of three functions:  
    * `on()`: start listening for events or a given type
    * `once()`: listen for the next occurrence of a given event, and then stop
    * `removeListener()`: stop listening for events of a given type

The `on()` and `once()` exports delegate to the corresponding function from
`event/core`, and use `bind()` to pass the `exports` object itself as
the `target` argument to the underlying function. The `removeListener()`
function is implemented by calling the underlying `off()` function.

We can use this module in the same way we use any other module that emits
module-level events, such as
[`private-browsing`](modules/sdk/private-browsing.html). For example,
we can adapt "main.js" as follows:

    var bookmarks = require("./bookmarks");

    function logAdded(uri) {
      console.log("added: " + uri);
    }

    function logVisited(uri) {
      console.log("visited: " + uri);
    }

    exports.main = function() {
      bookmarks.on("added", logAdded);
      bookmarks.on("visited", logVisited);
    };

    exports.onUnload = function() {
      bookmarks.removeListener("added", logAdded);
      bookmarks.removeListener("visited", logVisited);
    }

## Classes as Event Targets ##

Sometimes we want to emit events at the level of individual objects,
rather than at the level of the module.

To do this, we can inherit from the SDK's
[`EventTarget`](modules/sdk/event/target.html) class. `EventTarget`
provides an implementation of the functions needed to add and remove
event listeners: `on()`, `once()`, and `removeListener()`.

In this example, we could define a class `BookmarkManager` that inherits
from `EventTarget` and emits `added` and `visited` events.

Open "bookmarks.js" and replace its contents with this code:

    var { emit } = require("sdk/event/core");
    var { EventTarget } = require("sdk/event/target");
    var { Class } = require("sdk/core/heritage");
    var { merge } = require("sdk/util/object");

    var {Cc, Ci, Cu} = require("chrome");
    Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
    var bookmarkService = Cc["@mozilla.org/browser/nav-bookmarks-service;1"]
                              .getService(Ci.nsINavBookmarksService);

    function createObserver(target) {
       var bookmarkObserver = {
         onItemAdded: function(aItemId, aFolder, aIndex) {
           emit(target, "added", bookmarkService.getBookmarkURI(aItemId).spec);
         },
         onItemVisited: function(aItemId, aVisitID, time) {
           emit(target, "visited", bookmarkService.getBookmarkURI(aItemId).spec);
         },
         QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
       };
       bookmarkService.addObserver(bookmarkObserver, false);
    }

    var BookmarkManager = Class({
      extends: EventTarget,
      initialize: function initialize(options) {
        EventTarget.prototype.initialize.call(this, options);
        merge(this, options);
        createObserver(this);
      }
    });

    exports.BookmarkManager = BookmarkManager;

The code to interact with the Places API is the same here. However:

* we're now importing from four modules:
    * [`event/core`](modules/sdk/event/core.html) gives us
`emit()`: note that we don't need `on`, `once`, or `off`,
since we will use `EventTarget` for adding and removing listeners
    * [`event/target`](modules/sdk/event/target.html) gives us
`EventTarget`, which implements the interface for adding and removing
listeners
    * [`heritage`](modules/sdk/core/heritage.html) gives us
`Class()`, which we can use to inherit from `EventTarget`
    * `utils/object` gives us `merge()`, which just simplifies setting up the
`BookmarkManager`'s properties
* we use `Class` to inherit from `EventTarget`. In its `initialize()` function,
we:
    * call the base class initializer
    * use `merge()` to copy any supplied options into the newly created object
    * call `createObserver()`, passing in the newly created object as the
event target
* `createObserver()` is the same as in the previous example, except that in
`emit()` we pass the newly created `BookmarkManager` as the event target

To use this event target we can create it and call the `on()`, `once()`, and
`removeListener()` functions that it has inherited:

    var bookmarks = require("./bookmarks");
    var bookmarkManager = bookmarks.BookmarkManager({});

    function logAdded(uri) {
      console.log("added: " + uri);
    }

    function logVisited(uri) {
      console.log("visited: " + uri);
    }

    exports.main = function() {
      bookmarkManager.on("added", logAdded);
      bookmarkManager.on("visited", logVisited);
    };

    exports.onUnload = function() {
      bookmarkManager.removeListener("added", logAdded);
      bookmarkManager.removeListener("visited", logVisited);
    }

### Implementing "onEvent" Options ###

Finally, most event targets accept options of the form "onEvent", where
"Event" is the capitalized form of the event type. For example, you
can listen to the
[`Panel` object's `show` event](modules/sdk/panel.html#show)
either by calling:

    myPanel.on("show", listenerFunction);

or by passing the `onShow` option to `Panel`'s constructor:

    var myPanel = require("sdk/panel").Panel({
      onShow: listenerFunction,
      contentURL: "https://en.wikipedia.org/w/index.php"
    });

If your class inherits from `EventTarget`, options like this are automatically
handled for you. For example, given the implementation of `BookmarkManager`
above, your "main.js" could be rewritten like this:

    var bookmarks = require("./bookmarks");

    function logAdded(uri) {
      console.log("added: " + uri);
    }

    function logVisited(uri) {
      console.log("visited: " + uri);
    }

    var bookmarkManager = bookmarks.BookmarkManager({
      onAdded: logAdded,
      onVisited: logVisited
    });

    exports.onUnload = function() {
      bookmarkManager.removeListener("added", logAdded);
      bookmarkManager.removeListener("visited", logVisited);
    }
