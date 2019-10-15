.. _browserusagetelemetry:

=======================
Browser Usage Telemetry
=======================

The `BrowserUsageTelemetry.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/modules/BrowserUsageTelemetry.jsm>`_ module is the main module for measurements regarding the browser usage (e.g. tab and window counts, search counts, ...).

The measurements recording begins right after the ``SessionStore`` has finished restoring the session (i.e. restoring tabs/windows after Firefox starts).

Search telemetry
================
This module exposes the ``recordSearch`` method, which serves as the main entry point for recording search related Telemetry. It records only the search *counts* per engine and the origin of the search, but nothing pertaining the search contents themselves.

As the transition to the ``BrowserUsageTelemetry`` happens, the ``recordSearch`` calls are dispatched through `BrowserSearch.recordSearchInTelemetry <https://dxr.mozilla.org/mozilla-central/rev/3e73fd638e687a4d7f46613586e5156b8e2af846/browser/base/content/browser.js#3752>`_, that is called by all the search related UI components (urlbar, searchbar, context menu and about\:\* pages).

A list of the components recording search Telemetry can be found using the following `DXR search <https://dxr.mozilla.org/mozilla-central/search?q=recordSearchInTelemetry>`_.

Measured interactions
=====================
The usage telemetry module currently measures these interactions with the browser:

- *tab and window engagement*: counts the number of non-private tabs and windows opened in a subsession, after the session is restored (see e.g. ``browser.engagement.max_concurrent_tab_count``);
- *URI loads*: counts the number of page loads (doesn't track and send the addresses, just the counts) directly triggered by the users (see ``browser.engagement.total_uri_count``);
- *navigation events*: at this time, this only counts the number of time a page load is triggered by a particular UI interaction (e.g. by searching through the URL bar, see ``browser.engagement.navigation.urlbar``).


Please see `Scalars.yaml <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yaml>`_ for the full list of tracked interactions.
