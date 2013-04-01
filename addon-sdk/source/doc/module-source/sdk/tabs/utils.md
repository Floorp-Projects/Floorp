<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `tabs/utils` module contains low-level functions for working with
XUL [`tabs`](https://developer.mozilla.org/en-US/docs/XUL/tab) and the XUL [`tabbrowser`](https://developer.mozilla.org/en-US/docs/XUL/tabbrowser)
object.

<api name="activateTab">
@function
Set the specified tab as the active, or
[selected](https://developer.mozilla.org/en-US/docs/XUL/tabbrowser#p-selectedTab),
tab.
@param tab {tab}
A [XUL `tab` element](https://developer.mozilla.org/en-US/docs/XUL/tab)
to activate.
@param window {window}
A browser window.
</api>

<api name="getTabBrowser">
@function
Get the [`tabbrowser`](https://developer.mozilla.org/en-US/docs/XUL/tabbrowser)
element for the given browser window.
@param window {window}
A browser window.
@returns {tabbrowser}
</api>

<api name="getTabContainer">
@function
Get the `tabbrowser`'s
[`tabContainer`](https://developer.mozilla.org/en-US/docs/XUL/tabbrowser#p-tabContainer)
property.
@param window {window}
A browser window.
@returns {tabContainer}
</api>

<api name="getTabs">
@function
Returns the tabs for the specified `window`.

If you omit `window`, this function will return tabs
across all the browser's windows. However, if your add-on
has not opted into private browsing, then the function will
exclude all tabs that are hosted by private browser windows.

To learn more about private windows, how to opt into private browsing, and how
to support private browsing, refer to the
[documentation for the `private-browsing` module](modules/sdk/private-browsing.html).

@param window {nsIWindow}
Optional.
@returns {Array}
An array of [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab)
elements.
</api>

<api name="getActiveTab">
@function
Given a browser window, get the active, or
[selected](https://developer.mozilla.org/en-US/docs/XUL/tabbrowser#p-selectedTab), tab.
@param window {window}
A browser window.
@returns {tab}
The currently selected
[`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab).
</api>

<api name="getOwnerWindow">
@function
Get the browser window that owns the specified `tab`.
@param tab {tab}
A browser [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab).
@returns {window}
A browser window.
</api>

<api name="openTab">
@function
Open a new tab in the specified browser window.
@param window {window}
The browser window in which to open the tab.
@param url {String}
URL for the document to load.
@param options {object}
Options for the new tab. These are currently only applicable
to Firefox for Android.
  @prop inBackground {boolean}
  If `true`, open the new tab, but keep the currently selected tab selected.
  If `false`, make the new tab the selected tab.
  Optional, defaults to `false`.
  @prop pinned {boolean}
  Pin this tab. Optional, defaults to `false`.
@returns {tab}
The new [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab).
</api>

<api name="isTabOpen">
@function
Test whether the specified tab is open.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {boolean}
`true` if the tab is open, otherwise `false`.
</api>

<api name="closeTab">
@function
Close the specified tab.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
</api>

<api name="getURI">
@function
Get the specified tab's URI.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {String}
The current URI.
</api>

<api name="getTabBrowserForTab">
@function
Get the specified tab's [`tabbrowser`](https://developer.mozilla.org/en-US/docs/XUL/tabbrowser).
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {tabbrowser}
</api>

<api name="getBrowserForTab">
@function
Get the specified tab's [`browser`](https://developer.mozilla.org/en-US/docs/XUL/browser).
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {browser}
</api>

<api name="getTabTitle">
@function
Get the title of the document hosted by the specified tab, or the tab's
label if the tab doesn't host a document.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {String}
</api>

<api name="setTabTitle">
@function
Set the title of the document hosted by the specified tab, or the
tab's label if the tab doesn't host a document.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@param title {String}
The new title.
</api>

<api name="getTabContentWindow">
@function
Get the specified tab's content window.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {window}
</api>

<api name="getAllTabContentWindows">
@function
Get all tabs' content windows across all the browsers' windows.
@returns {Array}
Array of windows.
</api>

<api name="getTabForContentWindow">
@function
Get the tab element that hosts the specified content window.
@param window {window}
@returns {tab}
</api>

<api name="getTabURL">
@function
Get the specified tab's URL.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {String}
The current URI.
</api>

<api name="setTabURL">
@function
Set the specified tab's URL.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@param url {String}
</api>

<api name="getTabContentType">
@function
Get the [`contentType`](https://developer.mozilla.org/en-US/docs/DOM/document.contentType)
of the document hosted by the specified tab.
@param tab {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
@returns {String}
</api>

<api name="getSelectedTab">
@function
Get the selected tab for the specified browser window.
@param window {window}
@returns {tab}
A XUL [`tab`](https://developer.mozilla.org/en-US/docs/XUL/tab) element.
</api>

