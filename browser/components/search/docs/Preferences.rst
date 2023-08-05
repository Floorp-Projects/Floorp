Preferences
===========

This document describes preferences affecting Firefox's Search UI code. For information
on the toolkit search service, see the :doc:`/toolkit/search/Preferences` document.
Preferences that are generated and updated by code won't be described here.

User Exposed
------------
These preferences are exposed through the Firefox UI

browser.search.widget.inNavBar (boolean, default: false)
  Whether the search bar widget is displayed in the navigation bar.

Hidden
------
These preferences are normally hidden, and should not be used unless you really
know what you are doing.

browser.search.openintab (boolean, default: false)
  Whether or not items opened from the search bar are opened in a new tab.

browser.search.context.loadInBackground (boolean, default: false)
  Whether or not tabs opened from searching in the context menu are loaded in
  the foreground or background.
