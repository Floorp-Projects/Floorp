Crash reporting
===============

Firefox ships with an open-source crash reporting system. This system is
combination of projects:

-  `Google
   Breakpad <https://chromium.googlesource.com/breakpad/breakpad>`__
   client and server libraries
-  Mozilla-specific crash reporting user interface and bootstrap code
-  `Socorro <https://github.com/mozilla-services/socorro>`__ Collection
   and reporting server


Where did my crash get submitted?
---------------------------------

Crash data submitted using the Mozilla Crash Reporter is located on
`crash-stats <https://crash-stats.mozilla.com/>`__. If you want to find
a specific crash that you submitted, you first need to find the Crash ID
that the server has assigned your crash. Type ``about:crashes`` into
your location bar to get a page listing both submitted and unsubmitted
crash reports. For more information, see `How to get a stacktrace for a
bug report </en-US/docs/How_to_get_a_stacktrace_for_a_bug_report>`__.


Reports and queries
-------------------

crash-stats has built-in reports of "topcrashes" for each release
grouped by signature. There is also a custom query tool which allows
users to limit searches on more precise information.

Finally, a set of Mozilla employees have access to directly query the
underlying data in either SQL summary or using mapreduce on the storage
cluster. If you are interested in obtaining this advanced access,
contact `Gabriele Svelto <mailto:gsvelto@mozilla.com>`__.


See also
--------

-  `Understanding crash
   reports </en-US/docs/Mozilla/Projects/Crash_reporting/Understanding_crash_reports>`__
-  `A guide to searching crash
   reports </en-US/docs/Mozilla/Projects/Crash_reporting/Searching_crash_reports>`__
-  `crash-stats <https://crash-stats.mozilla.com/>`__
-  `Crash pings (Telemetry) and crash reports (Socorro/Crash
   Stats) <https://bluesock.org/~willkg/blog/mozilla/crash_pings_crash_reports.html>`__
-  `Building Firefox with Debug
   Symbols </en-US/docs/Building_Firefox_with_Debug_Symbols>`__
-  `Environment variables affecting crash
   reporting <https://firefox-source-docs.mozilla.org/toolkit/crashreporter/crashreporter/index.html#environment-variables-affecting-crash-reporting>`__
-  `Uploading Symbols to Mozilla's Symbol
   Server </en-US/docs/Uploading_symbols_to_Mozillas_symbol_server>`__
-  In-code documentation

   -  `Crash
      reporter <https://firefox-source-docs.mozilla.org/toolkit/crashreporter/crashreporter/index.html>`__
   -  `Crash
      manager <https://firefox-source-docs.mozilla.org/toolkit/components/crashes/crash-manager/index.html>`__
   -  `Crash
      ping <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/data/crash-ping.html>`__
