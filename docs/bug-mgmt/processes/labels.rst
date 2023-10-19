GitHub Metadata Recommendations
===============================

To have better consistency with code and task tracking among Mozilla
Central, Bugzilla, and GitHub, we request that you use a common set of
labels in your projects. Benefits of improved consistency in our
conventions include:

-  Consistency makes measurement of processes simpler across the
   organization
-  Consistency makes it easier to write reusable process tools
-  Consistency increases clarity for those than need to work across
   different repositories and bug trackers
-  Consistency reduces friction around engineering mobility between
   projects

We recommend creating sets of labels in your project to do this.

Bug types
---------

In Bugzilla bugs are distinguished by type: ``defect``, ``enhancement``,
and ``tasks``. Use a label to make this distinction in your project.

Statuses
--------

Bugs in GitHub issues have two states: closed and open. Bugzilla has a
richer set of states.

When you close a bug, add a label indicating `the
resolution <https://wiki.mozilla.org/BMO/UserGuide/BugStatuses#Resolutions>`__.

-  ``fixed``

   -  A change set for the bug has been landed in Mozilla-Central
   -  A GitHub issue could be closed, but the change set has not
      landed so it would be still considered open from the
      Bugzilla point of view

-  ``invalid``

   -  The problem described is not a bug.

-  ``incomplete``

   -  The problem is vaguely described with no steps to reproduce, or is
      a support request.

-  ``wontfix``

   -  The problem described is a bug which will never be fixed.

-  ``duplicate``

   -  The problem is a duplicate of an existing bug. Be sure to link the
      bug this is a duplicate of.

-  ``worksforme``

   -  All attempts at reproducing this bug were futile, and reading the
      code produces no clues as to why the described behavior would
      occur.

Severities (Required)
---------------------

The triage process for Firefox bugs in Bugzilla requires a non default
value of a bug's :ref:`Severity (definitions) <Defect Severity>`.

Release Status Flags
--------------------

Open Firefox bugs may also have :ref:`status flags <Release Status Flags>`
(``status_firefoxNN``) set for Nightly, Beta, Release, or ESR.

Priorities
----------

Firefox projects in Bugzilla can use the :ref:`priority field <Priority Definitions>`
to indicate when a bug will be worked on.

Keywords
--------

In GitHub issues metadata is either a label or the bug’s open/closed
state.

Some Bugzilla metadata behaves like labels, but you need to be careful
with how you use it in order not to confuse QA.

Regressions
~~~~~~~~~~~

In Bugzilla, the ``regression`` keyword indicates a regression in
existing behavior introduced by a code change.

When a bug is labeled as a regression in GitHub does it imply the
regression is in the code module in GitHub, or the module that’s landed
in Mozilla Central? Using the label ``regression-internal`` will signal
QA that the regression is internal to your development cycle, and not
one introduced into the Mozilla Central tree.

If it is not clear which pull request caused the regression, add the
``regressionwindow-wanted`` label.

Other Keywords
~~~~~~~~~~~~~~

Other useful labels include ``enhancement`` to distinguish feature
requests, and ``good first issue`` to signal to contributors (`along
with adequate
documentation <http://blog.humphd.org/why-good-first-bugs-often-arent/>`__.)

Summary
-------

To represent Bugzilla fields, use labels following this scheme.

-  Bug types

   -  ``defect``, ``enhancement``, ``task``

-  Resolution statuses

   -  ``invalid``, ``duplicate``, ``incomplete``, ``worksforme``,
      ``wontfix``

-  Regressions

   -  ``regression``, ``regressionwindow-wanted``,
      ``regression-internal``


-  :ref:`Severity <Defect Severity>` (required)

   -  ``S1``, ``S2``, ``S3``, ``S4``, ``N/A`` (reserved for bugs
      of type ``task`` or ``enhancement``)

-  :ref:`Status flags <Release Status Flags>`

   -  ``status_firefoxNN:<status>``
      (example ``status_firefox77:affected``)

-  :ref:`Priority <Priority Definitions>`

   -  ``P1``, ``P2``, ``P3``, ``P5``

-  Other keywords

   -  ``good first bug``, ``perf``, &etc.


You may already have a set of tags, so do an edit to convert them
or use `the GitHub settings app <https://github.com/probot/settings>`__.
