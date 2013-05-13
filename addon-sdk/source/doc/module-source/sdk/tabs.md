<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Dietrich Ayala [dietrich@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

The `tabs` module provides easy access to tabs and tab-related events.

The module itself can be used like a basic list of all opened
tabs across all windows. In particular, you can enumerate it:

    var tabs = require('sdk/tabs');
    for each (var tab in tabs)
      console.log(tab.title);

You can also access individual tabs by index:

    var tabs = require('sdk/tabs');

    tabs.on('ready', function () {
      console.log('first: ' + tabs[0].title);
      console.log('last: ' + tabs[tabs.length-1].title);
    });

You can open a new tab, specifying various properties including location:

    var tabs = require("sdk/tabs");
    tabs.open("http://www.example.com");

You can register event listeners to be notified when tabs open, close, finish
loading DOM content, or are made active or inactive:

    var tabs = require("sdk/tabs");

    // Listen for tab openings.
    tabs.on('open', function onOpen(tab) {
      myOpenTabs.push(tab);
    });

    // Listen for tab content loads.
    tabs.on('ready', function(tab) {
      console.log('tab is loaded', tab.title, tab.url)
    });

You can get and set various properties of tabs (but note that properties
 relating to the tab's content, such as the URL, will not contain valid
values until after the tab's `ready` event fires). By setting the `url`
property you can load a new page in the tab:

    var tabs = require("sdk/tabs");
    tabs.on('activate', function(tab) {
      tab.url = "http://www.example.com";
    });

You can attach a [content script](dev-guide/guides/content-scripts/index.html)
to the page hosted in a tab, and use that to access and manipulate the page's
content:

    var tabs = require("sdk/tabs");

    tabs.on('activate', function(tab) {
      tab.attach({
        contentScript: 'self.postMessage(document.body.innerHTML);',
        onMessage: function (message) {
          console.log(message);
        }
      });
    });

## Private Windows ##

If your add-on has not opted into private browsing, then you won't see any
tabs that are hosted by private browser windows.

Tabs hosted by private browser windows won't be seen if you enumerate the
`tabs` module itself, and you won't receive any events for them.

To learn more about private windows, how to opt into private browsing, and how
to support private browsing, refer to the
[documentation for the `private-browsing` module](modules/sdk/private-browsing.html).

<api name="activeTab">
@property {Tab}

The currently active tab in the active window. This property is read-only. To
activate a `Tab` object, call its `activate` method.

**Example**

    // Get the active tab's title.
    var tabs = require("sdk/tabs");
    console.log("title of active tab is " + tabs.activeTab.title);
</api>

<api name="length">
@property {number}
The number of open tabs across all windows.
</api>

<api name="open">
@function
Opens a new tab. The new tab will open in the active window or in a new window,
depending on the `inNewWindow` option.

**Example**

    var tabs = require("sdk/tabs");

    // Open a new tab on active window and make tab active.
    tabs.open("http://www.mysite.com");

    // Open a new tab in a new window and make it active.
    tabs.open({
      url: "http://www.mysite.com",
      inNewWindow: true
    });

    // Open a new tab on active window in the background.
    tabs.open({
      url: "http://www.mysite.com",
      inBackground: true
    });

    // Open a new tab as an app tab and do something once it's open.
    tabs.open({
      url: "http://www.mysite.com",
      isPinned: true,
      onOpen: function onOpen(tab) {
        // do stuff like listen for content
        // loading.
      }
    });

@param options {object}
An object containing configurable options for how and where the tab will be
opened, as well as a listeners for the tab events.

If the only option being used is `url`, then a bare string URL can be passed to
`open` instead of adding at a property of the `options` object.

@prop [url] {string}
String URL to be opened in the new tab.
This is a required property.

@prop [inNewWindow] {boolean}
If present and true, a new browser window will be opened and the URL will be
opened in the first tab in that window. This is an optional property.

@prop [inBackground] {boolean}
If present and true, the new tab will be opened to the right of the active tab
and will not be active. This is an optional property.

@prop isPrivate {boolean}
Boolean which will determine whether the new tab should be private or not.
If your add-on does not support private browsing this will have no effect.
See the [private-browsing](modules/sdk/private-browsing.html) documentation for more information.

@prop [isPinned] {boolean}
If present and true, then the new tab will be pinned as an
[app tab](http://support.mozilla.com/en-US/kb/what-are-app-tabs).

@prop [onOpen] {function}
A callback function that will be registered for 'open' event.
This is an optional property.
@prop [onClose] {function}
A callback function that will be registered for 'close' event.
This is an optional property.
@prop [onReady] {function}
A callback function that will be registered for 'ready' event.
This is an optional property.
@prop [onLoad] {function}
A callback function that will be registered for 'load' event.
This is an optional property.
@prop [onPageShow] {function}
A callback function that will be registered for 'pageshow' event.
This is an optional property.
@prop [onActivate] {function}
A callback function that will be registered for 'activate' event.
This is an optional property.
@prop [onDeactivate] {function}
A callback function that will be registered for 'deactivate' event.
This is an optional property.
</api>

<api name="Tab">
@class
A `Tab` instance represents a single open tab. It contains various tab
properties, several methods for manipulation, as well as per-tab event
registration.

Tabs emit all the events described in the Events section. Listeners are
passed the `Tab` object that triggered the event.

<api name="id">
@property {string}
The unique id for the tab. This property is read-only.
</api>

<api name="title">
@property {string}
The title of the tab (usually the title of the page currently loaded in the tab)
This property can be set to change the tab title.
</api>

<api name="url">
@property {String}
The URL of the page currently loaded in the tab.
This property can be set to load a different URL in the tab.
</api>

<api name="favicon">
@property {string}
The URL of the favicon for the page currently loaded in the tab.
This property is read-only.
</api>

<api name="contentType">
@property {string}
<div class="experimental">
<strong>
  This is currently an experimental API, so we might change it in future releases.
</strong>
<p>
Returns the MIME type that the document currently loaded in the tab is being
rendered as.
This may come from HTTP headers or other sources of MIME information,
and might be affected by automatic type conversions performed by either the
browser or extensions.
This property is read-only.
</p>
</div>
</api>

<api name="index">
@property {integer}
The index of the tab relative to other tabs in the application window.
This property can be set to change its relative position.
</api>

<api name="isPinned">
@property {boolean}
Whether or not tab is pinned as an [app tab][].
This property is read-only.
[app tab]:http://support.mozilla.com/en-US/kb/what-are-app-tabs
</api>

<api name="pin">
@method
Pins this tab as an [app tab][].
[app tab]:http://support.mozilla.com/en-US/kb/what-are-app-tabs
</api>

<api name="unpin">
@method
Unpins this tab.
</api>

<api name="close">
@method
Closes this tab.

@param [callback] {function}
A function to be called when the tab finishes its closing process.
This is an optional argument.
</api>

<api name="reload">
@method
Reloads this tab.
</api>

<api name="activate">
@method
Makes this tab active, which will bring this tab to the foreground.
</api>

<api name="getThumbnail">
@method
Returns thumbnail data URI of the page currently loaded in this tab.
</api>

<api name="attach">
@method
  Create a page mod and attach it to the document in the tab.

**Example**

    var tabs = require("sdk/tabs");

    tabs.on('ready', function(tab) {
      tab.attach({
          contentScript:
            'document.body.style.border = "5px solid red";'
      });
    });

@param options {object}
  Options for the page mod, with the following keys:

@prop [contentScriptFile] {string,array}
    The local file URLs of content scripts to load.  Content scripts specified
    by this option are loaded *before* those specified by the `contentScript`
    option. Optional.
@prop [contentScript] {string,array}
    The texts of content scripts to load.  Content scripts specified by this
    option are loaded *after* those specified by the `contentScriptFile` option.
    Optional.
@prop [onMessage] {function}
    A function called when the page mod receives a message from content scripts.
    Listeners are passed a single argument, the message posted from the
    content script. Optional.

@returns {Worker}
  See [Content Scripts guide](dev-guide/guides/content-scripts/index.html)
  to learn how to use the `Worker` object to communicate with the content script.

</api>

<api name="close">
@event

This event is emitted when the tab is closed.  It's also emitted when the
tab's window is closed.

@argument {Tab}
Listeners are passed the tab object.
</api>

<api name="ready">
@event

This event is emitted when the DOM for the tab's content is ready. It is
equivalent to the `DOMContentLoaded` event for the given content page.

A single tab will emit this event every time the DOM is loaded: so it will be
emitted again if the tab's location changes or the content is reloaded.

After this event has been emitted, all properties relating to the tab's
content can be used.

@argument {Tab}
Listeners are passed the tab object.
</api>

<api name="load">
@event

This event is emitted when the page for the tab's content is loaded. It is
equivalent to the `load` event for the given content page.

A single tab will emit this event every time the page is loaded: so it will be
emitted again if the tab's location changes or the content is reloaded.

After this event has been emitted, all properties relating to the tab's
content can be used.

This is fired after the `ready` event on DOM content pages and can be used
for pages that do not have a `DOMContentLoaded` event, like images.

@argument {Tab}
Listeners are passed the tab object.
</api>

<api name="pageshow">
@event

This event is emitted when the page for the tab's content is potentially
from the cache. It is equivilent to the [pageshow](https://developer.mozilla.org/en-US/docs/DOM/Mozilla_event_reference/pageshow) event for the given
content page.

After this event has been emitted, all properties relating to the tab's
content can be used.

While the `ready` and `load` events will not be fired when a user uses the back
or forward buttons to navigate history, the `pageshow` event will be fired.
If the `persisted` argument is true, then the contents were loaded from the
bfcache.

@argument {Tab}
Listeners are passed the tab object.
@argument {persisted}
Listeners are passed a boolean value indicating whether or not the page
was loaded from the [bfcache](https://developer.mozilla.org/en-US/docs/Working_with_BFCache) or not.
</api>

<api name="activate">
@event

This event is emitted when the tab is made active.

@argument {Tab}
Listeners are passed the tab object.
</api>

<api name="deactivate">
@event

This event is emitted when the tab is made inactive.

@argument {Tab}
Listeners are passed the tab object.
</api>

</api>

<api name="open">
@event

This event is emitted when a new tab is opened. This does not mean that
the content has loaded, only that the browser tab itself is fully visible
to the user.

Properties relating to the tab's content (for example: `title`, `favicon`,
and `url`) will not be correct at this point. If you need to access these
properties, listen for the `ready` event:

    var tabs = require("sdk/tabs");
    tabs.on('open', function(tab){
      tab.on('ready', function(tab){
        console.log(tab.url);
      });
    });

@argument {Tab}
Listeners are passed the tab object that just opened.
</api>

<api name="close">
@event

This event is emitted when a tab is closed. When a window is closed
this event will be emitted for each of the open tabs in that window.

@argument {Tab}
Listeners are passed the tab object that has closed.
</api>

<api name="ready">
@event

This event is emitted when the DOM for a tab's content is ready.
It is equivalent to the `DOMContentLoaded` event for the given content page.

A single tab will emit this event every time the DOM is loaded: so it will be
emitted again if the tab's location changes or the content is reloaded.

After this event has been emitted, all properties relating to the tab's
content can be used.

@argument {Tab}
Listeners are passed the tab object that has loaded.
</api>

<api name="activate">
@event

This event is emitted when an inactive tab is made active.

@argument {Tab}
Listeners are passed the tab object that has become active.
</api>

<api name="deactivate">
@event

This event is emitted when the active tab is made inactive.

@argument {Tab}
Listeners are passed the tab object that has become inactive.
</api>
