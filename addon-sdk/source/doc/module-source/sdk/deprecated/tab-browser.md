<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Dietrich Ayala [dietrich@mozilla.com]  -->

<div class="warning">
<p>The <code>tab-browser</code> module is deprecated.</p>
<p>For low-level access to tabs, use the
<a href="modules/sdk/tabs/utils.html"><code>tabs/utils</code></a>
module.</p>
</div>

The `tab-browser` module is a low-level API that provides privileged
access to browser tab events and actions.

Introduction
------------

The `tab-browser` module contains helpers for tracking tabbrowser elements
and tabs, as well as a few utilities for actions such as opening a new
tab, and catching all tab content loads.

This is a low-level API that has full privileges, and is intended to be used
by SDK internal modules. If you just need easy access to tab events for your
add-on, use the Tabs module (JEP 110).

<api name="activeTab">
@property {element}
The XUL tab element of the currently active tab.
</api>

<api name="addTab">
@function
Adds a new tab.

**Example**

    var tabBrowser = require("tab-browser");
    tabBrowser.addTab("http://google.com");

    var tabBrowser = require("tab-browser");
    tabBrowser.addTab("http://google.com", {
      inBackground: true
    });

    var tabBrowser = require("tab-browser");
    tabBrowser.addTab("http://google.com", {
      inNewWindow: true,
      onLoad: function(tab) {
        console.log("tab is open.");
      }
    });

@returns {element}
The XUL tab element of the newly created tab.

@param URL {string}
The URL to be opened in the new tab.

@param options {object}
Options for how and where to open the new tab.

@prop [inNewWindow] {boolean}
An optional parameter whose key can be set in `options`.
If true, the tab is opened in a new window. Default is false.

@prop [inBackground] {boolean}
An optional parameter whose key can be set in `options`.
If true, the tab is opened adjacent to the active tab, but not
switched to. Default is false.

@prop [onLoad] {function}
An optional parameter whose key can be set in `options`.
A callback function that is called once the tab has loaded.
The XUL element for the tab is passed as a parameter to
this function.
</api>

<api name="Tracker">
@function
Register a delegate object to be notified when tabbrowsers are created
and destroyed.

The onTrack method will be called once per pre-existing tabbrowser, upon
tracker registration.

**Example**

    var tabBrowser = require("tab-browser");
    let tracker = {
      onTrack: function(tabbrowser) {
        console.log("A new tabbrowser is being tracked.");
      },
      onUntrack: function(tabbrowser) {
        console.log("A tabbrowser is no longer being tracked.");
      }
    };
    tabBrowser.Tracker(tracker);

@param delegate {object}
Delegate object to be notified each time a tabbrowser is created or destroyed.
The object should contain the following methods:

@prop [onTrack] {function}
Method of delegate that is called when a new tabbrowser starts to be tracked.
The tabbrowser element is passed as a parameter to this method.

@prop [onUntrack] {function}
Method of delegate that is called when a tabbrowser stops being tracked.
The tabbrowser element is passed as a parameter to this method.
</api>

<api name="TabTracker">
@function
Register a delegate object to be notified when tabs are opened and closed.


The onTrack method will be called once per pre-existing tab, upon
tracker registration.

**Example**

    var tabBrowser = require("tab-browser");
    let tracker = {
      onTrack: function(tab) {
        console.log("A new tab is being tracked.");
      },
      onUntrack: function(tab) {
        console.log("A tab is no longer being tracked.");
      }
    };
    tabBrowser.TabTracker(tracker);

@param delegate {object}
Delegate object to be notified each time a tab is opened or closed.
The object should contain the following methods:

@prop [onTrack] {function}
Method of delegate that is called when a new tab starts to be tracked.
The tab element is passed as a parameter to this method.

@prop [onUntrack] {function}
Method of delegate that is called when a tab stops being tracked.
The tab element is passed as a parameter to this method.
</api>

