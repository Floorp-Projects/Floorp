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

Where to Start
--------------

If you want a high level, nontechnical summary of how the address bar works,
read :doc:`nontechnical-overview`.

If you are interested in the technical details, you might want to skip ahead to
:doc:`overview`.

Codebase
--------

The address bar code lives in `browser/components/urlbar <https://searchfox.org/mozilla-central/source/browser/components/urlbar/>`_.

Table of Contents
-----------------

.. toctree::

   nontechnical-overview
   overview
   lifetime
   utilities
   telemetry
   firefox-suggest-telemetry
   debugging
   ranking
   experiments
   dynamic-result-types
   preferences
   testing
   contact

API Reference
-------------

.. toctree::

   UrlbarController
   UrlbarInput
   UrlbarView
