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
read :ref:`Nontechnical Overview`.

If you are interested in the technical details, you might want to skip ahead to
:ref:`Architecture Overview`.

Codebase
--------

Most of the address bar code lives in `browser/components/urlbar <https://searchfox.org/mozilla-central/source/browser/components/urlbar/>`_.
A separate and important back-end piece currently is `toolkit/components/places/UnifiedComplete.jsm <https://searchfox.org/mozilla-central/source/toolkit/components/places/UnifiedComplete.jsm>`_, which was carried over from awesomebar and is
undergoing refactoring for quantumbar.

Table of Contents
-----------------

.. toctree::

   nontechnical-overview
   overview
   utilities
   telemetry
   debugging
   experiments
   dynamic-result-types
   contact
