Address Bar
===========

This document describes the implementation of Firefox's address bar, also known
as the quantumbar or urlbar. The address bar was also called the awesomebar
until Firefox 68, when it was substantially rewritten.

The address bar is a specialized search access point that aggregates data from
several different sources, including:

  * Places (Firefox's history and bookmarks system)
  * Search engines (including search suggestions)
  * WebExtensions
  * Open tabs

Most of the address bar code lives in `browser/components/urlbar <https://dxr.mozilla.org/mozilla-central/source/browser/components/urlbar/>`_.
A separate and important back-end piece currently is `toolkit/components/places/UnifiedComplete.jsm <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/places/UnifiedComplete.jsm>`_, which was carried over from awesomebar and has not yet been rewritten for quantumbar.

.. toctree::

   overview
   utilities
   telemetry
   debugging
   experiments
   contact
