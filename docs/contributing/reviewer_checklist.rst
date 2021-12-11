Reviewer Checklist
==================

   Submitting patches to Mozilla source code needn't be complex. This
   article provides a list of best practices for your patch content that
   reviewers will check for or require. Following these best practices
   will lead to a smoother, more rapid process of review and acceptance.


Good web citizenship
--------------------

-  Make sure new web-exposed APIs actually make sense and are either
   standards track or preffed off by default.
-  In C++, wrapper-cache as needed. If your object can be gotten from
   somewhere without creating it in the process, it needs to be
   wrapper-cached.


Correctness
-----------

-  The bug being fixed is a valid bug and should be fixed.
-  The patch fixes the issue.
-  The patch is not unnecessarily complicated.
-  The patch does not add duplicates of existing code ('almost
   duplicates' could mean a refactor is needed). Commonly this results
   in "part 0" of a bug, which is "tidy things up to make the fix easier
   to write and review".
-  If QA needs to verify the fix, you should provide steps to reproduce
   (STR).


Quality
-------

-  If you can unit-test it, you should unit-test it.
-  If it's JS, try to design and build so that xpcshell can exercise
   most functionality. It's quicker.
-  Make sure the patch doesn't create any unused code (e.g., remove
   strings when removing a feature)
-  All caught exceptions should be logged at the appropriate level,
   bearing in mind personally identifiable information, but also
   considering the expense of computing and recording log output.
   [Fennec: Checking for log levels is expensive unless you're using
   Logger.]


Style
-----

-  Follow the `style
   guide <https://firefox-source-docs.mozilla.org/code-quality/coding-style/index.html>`__
   for the language and module in question.
-  Follow local style for the surrounding code, even if that local style
   isn't formally documented.
-  New files have license declarations and modelines.
-  New JS files should use strict mode.
-  Trailing whitespace (git diff and splinter view both highlight this,
   as does hg with the color extension enabled). Whitespace can be fixed
   easily in Mercurial using the `CheckFiles
   extension <https://www.mercurial-scm.org/wiki/CheckFilesExtension>`__.
   In git, you can use git rebase --whitespace=fix.


Security issues
---------------

-  There should be no writing to arbitrary files outside the profile
   folder.
-  Be careful when reading user input, network input, or files on disk.
   Assume that inputs will be too big, too short, empty, malformed, or
   malicious.
-  Tag for sec review if unsure.
-  If you're writing code that uses JSAPI, chances are you got it wrong.
   Try hard to avoid doing that.


Privacy issues
--------------

-  There should be no logging of URLs or content from which URLs may be
   inferred.
-  [Fennec: Android Services has Logger.pii() for this purpose (e.g.,
   logging profile dir)].
-  Tag for privacy review if needed.


Resource leaks
--------------

-  In Java, memory leaks are largely due to singletons holding on to
   caches and collections, or observers sticking around, or runnables
   sitting in a queue.
-  In C++, cycle-collect as needed. If JavaScript can see your object,
   it probably needs to be cycle-collected.
-  [Fennec: If your custom view does animations, it's better to clean up
   runnables in onDetachFromWindow().]
-  Ensure all file handles and other closeable resources are closed
   appropriately.
-  [Fennec: When writing tests that use PaintedSurface, ensure the
   PaintedSurface is closed when you're done with it.]


Performance impact
------------------

-  Check for main-thread IO [Fennec: Android may warn about this with
   strictmode].
-  Remove debug logging that is not needed in production.


Threading issues
----------------

-  Enormous: correct use of locking and volatility; livelock and
   deadlock; ownership.
-  [Fennec: All view methods should be touched only on UI thread.]
-  [Fennec: Activity lifecycle awareness (works with "never keep
   activities"). Also test with oom-fennec
   (`https://hg.mozilla.org/users/blassey_mozilla.com/oom-fennec/) <https://hg.mozilla.org/users/blassey_mozilla.com/oom-fennec/%29>`__].


Compatibility
-------------

-  Version files, databases, messages
-  Tag messages with ids to disambiguate callers.
-  IDL UUIDs are updated when the interface is updated.
-  Android permissions should be 'grouped' into a common release to
   avoid breaking auto-updates.
-  Android APIs added since Froyo should be guarded by a version check.


Preffability
------------

-  If the feature being worked on is covered by prefs, make sure they
   are hooked up.
-  If working on a new feature, consider adding prefs to control the
   behavior.
-  Consider adding prefs to disable the feature entirely in case bugs
   are found later in the release cycle.
-  [Fennec: "Prefs" can be Gecko prefs, SharedPreferences values, or
   build-time flags. Which one you choose depends on how the feature is
   implemented: a pure Java service can't easily check Gecko prefs, for
   example.]


Strings
-------

-  There should be no string changes in patches that will be uplifted
   (including string removals).
-  Rev entity names for string changes.
-  When making UI changes, be aware of the fact that strings will be
   different lengths in different locales.


Documentation
-------------

-  The commit message should describe what the patch is changing (not be
   a copy of the bug summary). The first line should be a short
   description (since only the first line is shown in the log), and
   additional description, if needed, should be present, properly
   wrapped, in later lines.
-  Adequately document any potentially confusing pieces of code.
-  Flag a bug with dev-doc-needed if any addon or web APIs are affected.
-  Use Javadocs extensively, especially on any new non-private methods.
-  When moving files, ensure blame/annotate is preserved.


Accessibility
-------------

-  For HTML pages, images should have the alt attribute set when
   appropriate. Similarly, a button that is not a native HTML button
   should have role="button" and the aria-label attribute set.
-  [Fennec: Make sure contentDescription is set for parts of the UI that
   should be accessible]
