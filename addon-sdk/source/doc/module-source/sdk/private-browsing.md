<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Paul O'Shannessy [paul@oshannessy.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->
<!-- contributed by Irakli Gozalishvili [gozala@mozilla.com] -->

## Per-window private browsing ##

Firefox 20 introduces per-window private browsing. This means that private
browsing status is a property of an individual browser window.

The user enters
private browsing by opening a new private browser window. When they do this,
any existing non-private windows are kept open, so the user will typically
have both private and non-private windows open at the same time.

## Opting into private browsing ##

Add-ons built using the SDK must opt into private browsing by setting the
following key in their [`package.json`](dev-guide/package-spec.html) file:

<pre>"permissions": {"private-browsing": true}</pre>

If an add-on has not opted in, then the high-level SDK modules will not
expose private windows, or objects (such as tabs) that are associated
with private windows:

* the [`windows`](modules/sdk/windows.html) module will not list any
private browser windows, generate any events for private browser windows,
or let the add-on open any private browser windows

* the [`tabs`](modules/sdk/tabs.html) module will not list any tabs that
belong to private browser windows, and the add-on won't receive any events
for such tabs

* any [`widgets`](modules/sdk/widget.html) will not be displayed in
private browser windows

* any menus or menu items created using the
[`context-menu`](modules/sdk/context-menu.html) will not be shown in
context menus that belong to private browser windows

* the [`page-mod`](modules/sdk/page-mod.html) module will not attach
content scripts to documents belonging to private browser windows

* any [`panel`](modules/sdk/panel.html) objects will not be shown if the
active window is a private browser window

* the [`selection`](modules/sdk/selection.html) module will not include
any selections made in private browser windows

Add-ons that have opted in will see private windows, so they will need to
use the `private-browsing` module to check whether objects are private,
so as to avoid storing data derived from such objects.

Additionally, add-ons that use low-level modules such as
[`window/utils`](modules/sdk/window/utils.html) may see private browser
windows with certain functions, even if they have not explicitly opted
into private browsing.

## Respecting private browsing ##

The `private-browsing` module exports a single function
[`isPrivate()`](modules/sdk/private-browsing.html#isPrivate(object))
that takes an object, which may be a
[`BrowserWindow`](modules/sdk/windows.html#BrowserWindow),
[`tab`](modules/sdk/tabs.html#Tab), or
[`worker`](modules/sdk/content/worker.html#Worker),
as an argument. It returns `true` only if the object is:

* a private window, or
* a tab belonging to a private window, or
* a worker that's associated with a document hosted in a private window

Add-ons can use this API to decide whether or not to store user data.
For example, here's an add-on that stores the titles of tabs the user loads,
and uses `isPrivate()` to exclude the titles of tabs that were loaded into
private windows:

    var simpleStorage = require("simple-storage");
 
    if (!simpleStorage.storage.titles)
      simpleStorage.storage.titles = [];
 
    require("tabs").on("ready", function(tab) {
      if (!require("private-browsing").isPrivate(tab)) {
        console.log("storing...");
        simpleStorage.storage.titles.push(tab.title);
      }
      else {
        console.log("not storing, private data");
      }
    });

Here's an add-on that uses a [page-mod](modules/sdk/page-mod.html) to log
the content of pages loaded by the user, unless the page is private. In
the handler for the page-mod's [`attach`](modules/sdk/page-mod.html#attach)
event, it passes the worker into `isPrivate()`:

    var pageMod = require("sdk/page-mod");
    var privateBrowsing = require("sdk/private-browsing");

    var loggingScript = "self.port.on('log-content', function() {" +
                        "  console.log(document.body.innerHTML);" +
                        "});";

    function logPublicPageContent(worker) {
      if (privateBrowsing.isPrivate(worker)) {
        console.log("private window, doing nothing");
      }
      else {
        worker.port.emit("log-content");
      }
    }

    pageMod.PageMod({
      include: "*",
      contentScript: loggingScript,
      onAttach: logPublicPageContent
    });

## Tracking private-browsing exit ##

Sometimes it can be useful to cache some data from private windows while they
are open, as long as you don't store it after the private browsing windows
have been closed. For example, the "Downloads" window might want to display
all downloads while there are still some private windows open, then clean out
all the private data when all private windows have closed.

To do this with the SDK, you can listen to the system event named
"last-pb-context-exited":

    var events = require("sdk/system/events");

    function listener(event) {
      console.log("last private window closed");
    }

    events.on("last-pb-context-exited", listener);

## Working with Firefox 19 ##

In Firefox 19, private browsing is a global property for the entire browser.
When the user enters private browsing, the existing browsing session is
suspended and a new blank window opens. This window is private, as are any
other windows opened until the user chooses to exit private browsing, at which
point all private windows are closed and the user is returned to the original
non-private session.

When running on Firefox 19, `isPrivate()` will return true if and only if
the user has global private browsing enabled.

<api name="isPrivate">
@function
Function to check whether the given object is private. It takes an
object as an argument, and returns `true` only if the object is:

* a private [`BrowserWindow`](modules/sdk/windows.html#BrowserWindow) or
* a [`tab`](modules/sdk/tabs.html#Tab) belonging to a private window, or
* a [`worker`](modules/sdk/content/worker.html#Worker) that's associated
with a document hosted in a private window

@param [object] {any}
  The object to check. This may be a
[`BrowserWindow`](modules/sdk/windows.html#BrowserWindow),
[`tab`](modules/sdk/tabs.html#Tab), or
[`worker`](modules/sdk/content/worker.html#Worker).
</api>
