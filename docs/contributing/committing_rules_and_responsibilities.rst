Committing rules and responsibilities
=====================================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

Preparation
-----------

There are things you need to be sure of before you even attempt to check
in:

-  Your code must
   :ref:`compile <Building Firefox On Linux>` and `pass all the automated tests <https://developer.mozilla.org/docs/Mozilla/QA/Automated_testing>`__
   before you consider pushing changes. If you are at all unsure, verify
   your changes with the
   `mozilla-central <https://wiki.mozilla.org/Build:TryServer>`__.
   try server, as appropriate.
-  You need :ref:`code review <Code Review FAQ>`.
-  Depending on the stage of the development process, you may need
   `approval <https://wiki.mozilla.org/Tree_Rules>`__. Commits to trees
   where approval is required must have "a=" in the commit message
   followed by the name of the approver.
-  Code should be factored in such a way such that we can disable
   features which cause regressions, either by backout or via a kill
   switch/preference. Be especially careful when landing features which
   depend on other new features which may be disabled. Ask
   mozilla.dev.planning for assistance if there are any questions.

Checkin comment
---------------

The checkin comment for the change you push should include the bug
number, the names of the reviewers, and a clear explanation of the fix.
Please say what changes are made, not what problem was fixed, e.g.:

Good: "Bug 123456 - Null-check presentation shell so we don't crash when a
button removes itself during its own onclick handler. r=paul, a=ringo."

Bad: "Bug 123456 - crash clicking button on www.example.com"

If you are not the author of the code, use ``hg commit -u`` to specify
the actual author in the Mercurial changeset:

::

   hg commit -u "Pat Chauthor <pat@chauthor.com>"

Commit message restrictions
---------------------------

The purpose of these new restrictions, implemented via a mercurial hook,
is to prevent commit messages that do not have a bug number. We will
still allow a small set of special commits lacking bugs numbers, like
merges and backouts.

This hook will be enabled on mozilla-central and every major branch that
directly merges into it, such as autoland or integration
branches, team branches, or established project branches.

An example for a passing commit message would be,

::

   Bug 577872 - Create WebM versions of Ogg reftests. r=kinetik

Note the *Bug ####*, you at least need that. You also can't commit
bustage-fixes without a bug number anymore. This is intentional to keep
track of the bug which caused it.

Allowed are:

-  Commit messages containing "bug" or "b=" followed by a bug number
-  Commit messages containing "no bug" (please use this sparingly)
-  Commit message indicating backout of a given 12+ digit changeset ID,
   starting with (back out|backing out|backed out|backout)( of)?
   (rev|changeset|cset)s? [0-9a-f]{12}
-  Commit messages that start with "merge" or "merging" and are actually
   for a merge changeset.

Special exceptions:

-  Commits by the special users "ffxbld", "seabld", "tbirdbld", or
   "cltbld".
-  When the commit is older then some date shortly after the hook has
   been enabled, to allow merges from other branches. This exception
   will be lifted after a short period of time (probably a few months)
   after the hooks is enabled.
-  You can also specify "IGNORE BAD COMMIT MESSAGES" in the tip (latest)
   commit message to override all the restrictions. This is an extreme
   measure, so you should only do this if you have a very good reason.

Explicitly disallowed:

-  Commit messages containing "try: " to avoid unintentional commits
   that were meant for the try server.

All tests for allowed or excluded messages are case-insensitive. The
hook,
`commit-message.py <https://hg.mozilla.org/hgcustom/version-control-tools/file/tip/hghooks/mozhghooks/commit-message.py>`__,
was added in `bug 506949 <https://bugzilla.mozilla.org/show_bug.cgi?id=506949>`__.


Check the tree
--------------

TaskCluster is a continuous build system that builds and tests every change
checked into autoland/mozilla-central and related source trees.
`Treeherder <https://treeherder.mozilla.org/>`__ displays the progress
and results of all the build and test jobs for a given tree. For a
particular job, green means all is well, orange means tests have failed,
and red means the build itself broke. Purple means that a test was
interrupted, possibly by a problem with the build system or the
network. Blue means that a test was interrupted in a known way and will
be automatically restarted. You can click on the "Help" link in the top
right corner of Treeherder for a legend to help you decode all the other
colors and letters.

If the tree is green, it is okay to check in. If some builds are orange
or red, you can either wait, or make sure all the failures are
classified with annotations/comments that reference bug numbers or
fixes.

If the tree is marked as "closed", or if you have questions about any
oranges or reds, you should contact the sheriff before checking in.


Failures and backouts
---------------------

Patches which cause unit test failures (on :ref:`tier 1
platforms <Supported Build Hosts and Targets>`) will be backed out.
Regressions on tier-2 platforms and in performance are not cause for a
direct backout, but you will be expected to help fix them if quickly.

*Note: Performance regressions require future data points to ensure a
sustained regression and can take anywhere from 3 hours to 30 hours
depending on the volume of the tree and build frequency. All regression
alerts do get briefly investigated and bugs are filed if necessary.*


Dealing with test failures
~~~~~~~~~~~~~~~~~~~~~~~~~~

If a build or a test job fails, you can click on the red or orange or
purple symbol for the job on Treeherder to display more information. 
The information will appear in the footer, including a summary of any
error messages, a "+" icon to re-trigger the job (schedule it to run
again), and links to the log files and to possibly-related bugs.

Here are some steps you can follow to figure out what is causing most
failures, `and "star" them
appropriately <http://ehsanakhgari.org/blog/2010-04-09/assisted-starring-oranges>`__:

#. Click on the failing job to see a list of suggested bugs. If the
   failure clearly matches a known bug, **click on the star** next to
   that bug and then click "Add a comment" and then submit the comment.
   This is referred to as "starring the build;" you'll see this phrase
   or ones like it in IRC a lot.
#. If the failure might match a known bug but you are not sure, click
   the bug number to open the Bugzilla report, and click the failing job
   to open its log. If the log and the bug do match, add a comment as
   in step 1 (above).
#. If the summary does not seem to match any suggested bugs, search
   Bugzilla for the name of the failing test or the error message. If
   you find a matching bug, add a comment in the bug in Bugzilla, and
   another to the job in Treeherder.
#. If you can't figure out whether a known bug exists (for example,
   because you can't figure out what part of the log you should search
   for), look on Treeherder to see if there are other similar failures
   nearby, or ask on #developers to see if anyone recognizes it as a
   known failure. For example, many Android tests fail frequently in
   ways that do not produce useful log messages. You can often find the
   appropriate bug just by looking at other Android failures that are
   already starred.
#. If there is no matching bug, you can back out the change (if you
   suspect the failure was caused by your changeset) or re-trigger the
   job (if you suspect it's an unrelated intermittent failure). After
   more test runs it should become clear whether it is a new regression
   or just an unknown intermittent failure.
#. If it turns out to be an unknown intermittent failure, file a new bug
   with "intermittent-failure" in the keywords. Include the name of the
   test file and an one-line summary of the log messages in the Summary
   field. In the description, include an excerpt of the error messages
   from the log, and a link to the log file itself.

At any point if you are not sure or can't figure out what to do, ask for
advice or help in `#developers <https://chat.mozilla.org>`__.
If a large number of jobs are failing and you suspect an infrastructure problem, you can also ask
about it in `#releng <https://chat.mozilla.org>`__.


Dealing with performance regressions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Under some circumstances, if your patch causes a performance regression
that is not acceptable, it will get backed out.
