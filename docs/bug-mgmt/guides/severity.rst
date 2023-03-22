Defect Severity
===============

Definition
----------

We use the ``severity`` field in Bugzilla to indicate the scope of a
bug's effect on Firefox.

The field is display alongside the bug's priority.

Values
------

Severity levels and their definitions are enumerated at https://wiki.mozilla.org/BMO/UserGuide/BugFields#bug_severity.

By default, new bugs have a severity of ``--``.

Examples of S1 bugs
^^^^^^^^^^^^^^^^^^^

-  WebExtensions disabled for all users
-  Web search not working from URL bar
-  Crashes with data loss

Examples of S2 bugs
^^^^^^^^^^^^^^^^^^^

Bugs that could reasonably be expected to cause a Firefox user to switch browsers,
either because the severity is bad enough, or the frequency of occurrence is high enough.

-  `Bug 1640441 <https://bugzilla.mozilla.org/show_bug.cgi?id=1640441>`__ - Slack hangs
   indefinitely in a onResize loop
-  `Bug 1645651 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645651>`__ - Changes in
   Reddit's comment section JS code makes selecting text slow on Nightly

Bugs involving contractual partners (if not an S1)

Bugs reported from QA

-  `Bug 1640913 <https://bugzilla.mozilla.org/show_bug.cgi?id=1640913>`__ - because an
   important message is not visible with the dark theme. It's not marked as S1 since the
   issue is reproducible only on one OS and the functionality is not affected.
-  `Bug 1641521 <https://bugzilla.mozilla.org/show_bug.cgi?id=1641521>`__ - because videos
   are not working on several sites with ETP on (default). This is not an S1 since turning
   ETP off fixes the issue.

Fuzzblocker bugs, which prevent fuzzing from making progress

Examples of S3 bugs
^^^^^^^^^^^^^^^^^^^

Bugs filed by contributors as part of daily refactoring and maintenance of the code base.

`Bug 1634171 <https://bugzilla.mozilla.org/show_bug.cgi?id=1634171>`__ - Visual artifacts around circular images

Bugs reported from QA

-  `Bug 1635105 <https://bugzilla.mozilla.org/show_bug.cgi?id=1635105>`__ because
   the associated steps to reproduce are uncommon,
   and the issue is no longer reproducible after refresh.
-  `Bug 1636063 <https://bugzilla.mozilla.org/show_bug.cgi?id=1636063>`__ since it's
   reproducible only on a specific web app, and only with a particular set of configurations.


Rules of thumb
--------------

-  The severity of most bugs of type ``task`` and ``enhancement`` will be
   ``N/A``
-  **Do not** assign bugs of type ``defect`` the severity ``N/A``
