<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `window/utils` module provides helper functions for working with
application windows.

## Private Windows ##

With this module your add-on will see private browser windows
even if it has not explicitly opted into private browsing, so you need
to take care not to store any user data derived from private browser windows.

The exception is the [`windows()`](modules/sdk/window/utils.html#windows())
function which returns an array of currently opened windows. Private windows
will not be included in this list if your add-on has not opted into private
browsing.

To learn more about private windows, how to opt into private browsing, and how
to support private browsing, refer to the
[documentation for the `private-browsing` module](modules/sdk/private-browsing.html).

<api name="getMostRecentBrowserWindow">
  @function
  Get the topmost browser window, as an
  [`nsIDOMWindow`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIDOMWindow) instance.
  @returns {nsIDOMWindow}
</api>

<api name="getInnerId">
  @function
  Returns the ID of the specified window's current inner window.
  This function wraps
[`nsIDOMWindowUtils.currentInnerWindowID`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIDOMWIndowUtils#Attributes).
  @param window {nsIDOMWindow}
  The window whose inner window we are interested in.
  @returns {ID}
</api>

<api name="getOuterId">
  @function
  Returns the ID of the specified window's outer window.
  This function wraps
[`nsIDOMWindowUtils.outerWindowID`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIDOMWIndowUtils#Attributes).
  @param window {nsIDOMWindow}
  The window whose outer window we are interested in.
  @returns {ID}
</api>

<api name="getXULWindow">
  @function
  Returns the
  [`nsIXULWindow`](https://developer.mozilla.org/en/nsIDOMWindow) for the given
  [`nsIDOMWindow`](https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIXULWindow):

      var { Ci } = require('chrome');
      var utils = require('sdk/window/utils');
      var active = utils.getMostRecentBrowserWindow();
      active instanceof Ci.nsIXULWindow // => false
      utils.getXULWindow(active) instanceof Ci.nsIXULWindow // => true

  @param window {nsIDOMWindow}
  @returns {nsIXULWindow}
</api>

<api name="getBaseWindow">
  @function
  Returns the
  [`nsIBaseWindow`](http://mxr.mozilla.org/mozilla-central/source/widget/nsIBaseWindow.idl)
  for the given [`nsIDOMWindow`](https://developer.mozilla.org/en/nsIDOMWindow):

      var { Ci } = require('chrome');
      var utils = require('sdk/window/utils');
      var active = utils.getMostRecentBrowserWindow();
      active instanceof Ci.nsIBaseWindow // => false
      utils.getBaseWindow(active) instanceof Ci.nsIBaseWindow // => true

  @param window {nsIDOMWindow}
  @returns {nsIBaseWindow}
</api>

<api name="getToplevelWindow">
  @function
  Returns the toplevel
  [`nsIDOMWindow`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIDOMWindow)
  for the given child [`nsIDOMWindow`](https://developer.mozilla.org/en/nsIDOMWindow):

      var { Ci } = require('chrome');
      var utils = require('sdk/window/utils');
      var browserWindow = utils.getMostRecentBrowserWindow();
      var window = browserWindow.content; // `window` object for the current webpage
      utils.getToplevelWindow(window) == browserWindow // => true

  @param window {nsIDOMWindow}
  @returns {nsIDOMWindow}
</api>

<api name="getWindowDocShell">
  @function
  Returns the
[`nsIDocShell`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIDocShell)
  for the [tabbrowser](https://developer.mozilla.org/en-US/docs/XUL/tabbrowser)
element.
  @param window {nsIDOMWindow}
  @returns {nsIDocShell}
</api>

<api name="getWindowLoadingContext">
  @function
  Returns the `nsILoadContext`.
  @param window {nsIDOMWindow}
  @returns {nsILoadContext}
</api>

<api name="backgroundify">
  @function
  This function takes the specified `nsIDOMWindow` and
  removes it from the application's window registry, so it won't appear
  in the OS specific window lists for the application.

      var { backgroundify, open } = require('sdk/window/utils');
      var bgwin = backgroundify(open('data:text/html,Hello backgroundy'));

  Optionally more configuration options may be passed via a second
  `options` argument. If `options.close` is `false`, the unregistered
  window won't automatically be closed on application quit, preventing
  the application from quitting. You should make sure to close all such
  windows manually.

      var { backgroundify, open } = require('sdk/window/utils');
      var bgwin = backgroundify(open('data:text/html,Foo'), {
        close: false
      });

  @param window {nsIDOMWindow}
  @param options {object}
    @prop close {boolean}
    Whether to close the window on application exit. Defaults to `true`.
</api>

<api name="open">
  @function
  This function is used to open top level (application) windows.
  It takes the `uri` of the window document as its first
  argument and an optional hash of `options` as its second argument.

      var { open } = require('sdk/window/utils');
      var window = open('data:text/html,Hello Window');

  This function wraps [`nsIWindowWatcher.openWindow`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIWindowWatcher#openWindow%28%29).
  @param uri {string}
  URI of the document to be loaded into the window.
  @param options {object}
  Options for the function, with the following properties:
    @prop parent {nsIDOMWindow}
    Parent for the new window. Optional, defaults to `null`.
    @prop name {String}
    Name that is assigned to the window. Optional, defaults to `null`.
    @prop features {Object}
    Map of features to set for the window, defined like this:
    `{ width: 10, height: 15, chrome: true }`. See the
    [`window.open` features documentation](https://developer.mozilla.org/en/DOM/window.open#Position_and_size_features)
    for more details.

        var { open } = require('sdk/window/utils');
        var window = open('data:text/html,Hello Window', {
          name: 'jetpack window',
          features: {
            width: 200,
            height: 50,
            popup: true
          }
        });

    Optional, defaults to an empty map: `{}`.
  @returns {nsIDOMWindow}
</api>

<api name="openDialog">
  @function
  Opens a top level window and returns its `nsIDOMWindow` representation.
  This is the same as `open`, but you can supply more features.
  It wraps [`window.openDialog`](https://developer.mozilla.org/en-US/docs/DOM/window.openDialog).
  @param options {object}
  Options for the function, with the following properties:
    @prop url {String}
    URI of the document to be loaded into the window.
    Defaults to `"chrome://browser/content/browser.xul"`.
    @prop name {String}
    Optional name that is assigned to the window. Defaults to `"_blank"`.
    @prop features {Object}
    Map of features to set for the window, defined like:
    `{ width: 10, height: 15, chrome: true }`. For the set of features
    you can set, see the [`window.openDialog`](https://developer.mozilla.org/en-US/docs/DOM/window.openDialog)
    documentation. Optional, defaults to: `{'chrome,all,dialog=no'}`.
  @returns {nsIDOMWindow}
</api>

<api name="windows">
  @function
  Returns an array of all currently opened windows.
  Note that these windows may still be loading.

  If your add-on has not
  [opted into private browsing](modules/sdk/private-browsing.html),
  any private browser windows will not be included in the array.

  @returns {Array}
  Array of `nsIDOMWindow` instances.
</api>

<api name="isDocumentLoaded">
  @function
  Check if the given window's document is completely loaded.
  This means that its "load" event has been fired and all content
  is loaded, including the whole DOM document, images,
  and any other sub-resources.
  @param window {nsIDOMWindow}
  @returns {boolean}
  `true` if the document is completely loaded.
</api>

<api name="isBrowser">
  @function
  Returns true if the given window is a Firefox browser window:
  that is, its document has a `"windowtype"` of `"chrome://browser/content/browser.xul"`.
  @param window {nsIDOMWindow}
  @returns {boolean}
</api>

<api name="getWindowTitle">
  @function
  Get the [title](https://developer.mozilla.org/en-US/docs/DOM/document.title)
  of the document hosted by the given window
  @param window {nsIDOMWindow}
  @returns {String}
  This document's title.
</api>

<api name="isXULBrowser">
  @function
  Returns true if the given window is a XUL window.
  @param window {nsIDOMWindow}
  @returns {boolean}
</api>

<api name="getFocusedWindow">
  @function
  Gets the child window within the topmost browser window that is focused.
  See
  [`nsIFocusManager`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIFocusManager)
  for more details.
  @returns {nsIDOMWindow}
</api>

<api name="getFocusedElement">
  @function
  Get the element that is currently focused.
  This will always be an element within the document
  loaded in the focused window, or `null` if no element in that document is
  focused.
  @returns {nsIDOMElement}
</api>

<api name="getFrames">
  @function
  Get the frames contained by the given window.
  @param window {nsIDOMWindow}
  @returns {Array}
  Array of frames.
</api>


