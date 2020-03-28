Triage for Bugzilla
===================

Expectations
------------

All teams working on Firefox using either or both Mozilla-central and
Bugzilla are expected to follow the following process.

Why Triage
----------

We want to make sure that we looked at every defect and a priority has
been defined. This way, we make sure that we did not miss any critical
issues during the development and stabilization cycles.

Staying on top of the bugs in your component means:

-  You get ahead of critical regressions and crashes which could trigger
   a point release if uncaught

   -  And you don’t want to spend your holiday with the Release
      Management team (not that they don’t like you)

-  Your bug queue is not overwhelming

   -  Members of your team do not see the bug queue and get the
      ‘wiggins’

Who Triages
-----------

Engineering managers and directors are responsible for naming the
individuals responsible for triaging `defect and task
bugs <bug-types>`__ in a component.

We use Bugzilla to track this. See the `list of triage
owners <https://bugzilla.mozilla.org/page.cgi?id=triage_owners.html>`__.

If you need to change who is responsible for triaging a bug in a
component, please `file a bug against bugzilla.mozilla.org in the
Administration
component <https://bugzilla.mozilla.org/enter_bug.cgi?product=bugzilla.mozilla.org&component=Administration>`__.
When a new component is created, a triage owner **must** be named.

Rotating triage
~~~~~~~~~~~~~~~

Some components are monitored by a rotation of triagers. In those cases,
the triage owner should be seen as the person responsible for assuring
the component is triaged, but the work is done by the people in the
rotation. The `rotations are managed as
calendars <https://github.com/mozilla/relman-auto-nag/tree/master/auto_nag/scripts/configs>`__.

If you wish to set up a rotation for triaging one or more components,
contact the Bugzilla team on Slack (#bmo.)

Firefox::General and Toolkit::General
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Bugs in Firefox::General are fitted with Bug Bug’s model to see if
there’s another component with a high liklihood of fit, and if a
threshold confidence is achieved, the bug is moved to that component.

Members of the community also review bugs in this component and try to
move them to a component.

There are also two, rotating triagers for the component. They review the
remaining bugs over the course of a month and move them if possible.

To volunteer for this rotation, contact the Bugzilla team on Slack
(#bmo.)

What Do You Triage
------------------

As a triage owner the queries you should be following for your component
are:

-  All open bugs, in your components without a pending ``needinfo`` flag
   or a ``stalled`` keyword since start of current release cycle which
   do not have a priority set
-  All bugs with active review requests in your component which have not
   been modified in five days
-  All bugs with reviewed, but unlanded patches in your components
-  All bugs with a needinfo request unanswered for more than 10 days

There’s a tool to help you find bugs
https://mozilla.github.io/triage-center/ and the source is at
https://github.com/mozilla/triage-center/.

These bugs are reviewed in the weekly Regression Triage meeting \* Bugs
of type ``defect`` with the ``regression`` keyword without
-status-firefoxNN decisions \* Bugs of type ``defect`` with the
``regression`` keyword without a regression range

If a bug is an enhancement it needs a priority set and a target release
or program milestone. These bugs are normally reviewed by product
managers. Enhancements can lead to release notes and QA needed that we
also need to know about

If a bug is a task resulting in a changeset, release managers will need
to known when this work will be done. A task such as refactoring fragile
code can be risky.

Weekly or More Frequently (depending on the component) find `un-triaged
bugs in the components you
triage <https://bugzilla.mozilla.org/buglist.cgi?f1=triage_owner&bug_type=defect&o1=equals&resolution=---&priority=--&v1=%25user%25>`__.

For each bug decide :ref:`priority <Priority definitions>` (you can override what’s already been set,
as a triage lead, you are the decider.)

What About Release Status (status_firefoxNN) Flags
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Release status (and tracking) flags are set independent of triage
decisions, but should be consistent with triage decisions.

Also, as a release approaches, the release status of open, high priority
(P1) bugs can change. Triagers and engineering leads must review open
bugs with a release status of affected and fix_optional, and decide
which of these to keep, and move the others to wontfix for the current
cycle.

The flag ``status_firefoxNN`` has many values, here’s a cheat sheet.

== ========== ========== ============ =================
—  ?          unaffected affected     fixed
== ========== ========== ============ =================
?  unaffected            wontfix      verified
\  affected              fix-optional disabled
\                        fixed        verified-disabled
== ========== ========== ============ =================

The headers of the table are values of the status flag. Each column are
the states reachable from the column headings.

-  “—” we don’t know whether Firefox N is affected
-  “?” we don’t know whether Firefox N is affected, but we want to find
   out.
-  “affected” - present in this release
-  “unaffected” - not present in this release
-  “fixed” - a contributor has landed a fix to address the issue
-  “verified” - the fix has been verified by QA or other contributors
-  “disabled” - the fix or the feature has been backed out or disabled
-  “verified disabled” - QA or other contributors confirmed the fix or
   the feature has been backed out or disabled
-  “wontfix” - we have decided not to accept/uplift a fix for this
   release cycle (it is not the same as the bug resolution WONTFIX).
   This can also mean that we don’t know how to fix that and will ship
   with this bug
-  “fix-optional” - we would take a fix for the current release but
   don’t consider it as important/blocking for the release

Automatic Triage Overrides
~~~~~~~~~~~~~~~~~~~~~~~~~~

When a bug is tracked for a release, i.e. the ``tracking_firefoxNN``
flag is set to ``+`` or ``blocking`` triage decisions will be overridden,
or made as follows:

-  If a bug is tracked for or blocking beta, release or ESR, its
   priority will be set to ``P1``
-  If a bug is tracked for or blocking nightly, its priority will be set
   to ``P2``

Because bugs can be bumped in priority it’s essential that triage owners
review their
`P1 <https://bugzilla.mozilla.org/buglist.cgi?priority=P1&f1=triage_owner&o1=equals&resolution=---&v1=%25user%25>`__
and
`P2 <https://bugzilla.mozilla.org/buglist.cgi?priority=P2&f1=triage_owner&o1=equals&resolution=---&v1=%25user%25>`__
bugs frequently.

Questions and Edge Cases
~~~~~~~~~~~~~~~~~~~~~~~~

This bug is a feature request
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set the bug’s type to ``enhancement``, add the ``feature`` keyword if
relevant, and state to ``NEW``. This bug will be excluded from future
triage queries.

This bug is a task, not a defect
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set the bug’s type to ``task``, and state to ``NEW``. This bug will be
excluded from future triage queries.

If you are not sure of a bug’s type, check `our rules for bug
types <task-defect-enhancement>`__.

This bug’s state is ``UNCONFIRMED``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Are there steps to reproduce? If not, needinfo the person who filed the
bug, requesting steps to reproduce. You are not obligated to wait
forever for a response, and bugs for which open requests for information
go unanswered can be ``RESOLVED`` as ``INCOMPLETE``.

I need help reproducing the bug
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set a needinfo for the QA managers, Softvision project managers, or the
QA owner of the component of the bug.

I don’t have enough information to make a decision
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you don’t have a reproduction or confirmation, or have questions
about how to proceed, ``needinfo`` the person who filed the bug, or
someone who can answer.

The ``stalled`` keyword
^^^^^^^^^^^^^^^^^^^^^^^

The extreme case of not-enough-information is one which cannot be
answered with a ``needinfo`` request. The reporter has shared all they
know about the bug, we are out of strategies to take to resolve it, but
the bug should be kept open.

Mark the bug as stalled by adding the ``stalled`` keyword to it. The
keyword will remove it from the list of bugs to be triaged.

If a patch lands on a ``stalled`` bug, automation will remove the
keyword. Otherwise, when the ``keyword`` is removed, the bug will have
its priority reset to ``--`` and the components triage owner notified by
automation.

Bugs which remain ``stalled`` for long periods of time should be
reviewed, and closed if necessary.

This doesn’t fit into a P1, P2, P3, P4, or P5 framework
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Mark it as a P3.

If it’s a tracking bug, make sure has “[meta]” in the title and has the
``meta`` keyword added. This will remove it from the list of untriaged
bugs.

Bug is in the wrong Component
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Remove any priority set, then either move to what you think is the
correct component, or needinfo the person responsible for the component
to ask them.

I don’t think we should work on it at any time
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you’ll accept a patch, mark it as P5, otherwise, close it as WONTFIX

My project is on GitHub
^^^^^^^^^^^^^^^^^^^^^^^

We have `a guide for GitHub projects to follow </labels>`__ when
triaging.

Watch open needinfo flags
~~~~~~~~~~~~~~~~~~~~~~~~~

Don’t let open needinfo flags linger for more than two weeks.

Close minor bugs with unresponded needinfo flags.

Follow up on needinfo flag requests.

The tool will help you find these (the query is imperfect.)

End of Iteration/Release Cycle
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Review P1s
^^^^^^^^^^

Are there unresolved P1s?

Revisit their priority, and move to backlog (P3.)

Review P2s
^^^^^^^^^^

Are there P2s that should move to P1s for the next cycle?

Are there P2s you now know are lower priority, move to P3.

Review P3s
^^^^^^^^^^

Are there P3 bugs you now know you won’t get to? Either demote to P5
(will accept patch) or resolve as WONTFIX.

Tools
-----

Triage with me
~~~~~~~~~~~~~~

   One tool we use in addons is triage-with-me. Its a Firefox Add-on
   that sends all the pages you click on in bugzilla into a server which
   then sends the URL to everyone else in the triage. – Andy McKay

The upshot is, one person clicks on links in Bugzilla, the bugs open up
on everyone else’s computer.

-  https://addons.mozilla.org/en-US/firefox/addon/triage-with-me/.
-  http://www.agmweb.ca/2013-09-06-triage/
-  http://www.agmweb.ca/2015-03-10-triage-with-me-update/

Questions
---------

-  Ask in #bugmasters on irc.mozilla.org
-  Email emceeaich@mozilla.com
