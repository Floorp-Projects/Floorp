Triage for Bugzilla
===================

Expectations
------------

All teams working on Firefox using either or both Mozilla-central and
Bugzilla are expected to follow the following process.

What is a Triaged Bug
---------------------

The new definition of Triaged will be Firefox-related bugs of type
``defect`` where the component is not
``UNTRIAGED``, and a non-default value not equal to ``--`` or ``N/A``.

Bugs of type Task or Enhancement may have a severity of ``N/A``,
but defects must have a severity that is neither ``--`` or
``N/A``.

Why Triage
----------

We want to make sure that we looked at every defect and a severity has
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
individuals responsible for triaging :ref:`all types of bugs <Bug Types>` in a component.

We use Bugzilla to manage this. See the `list of triage
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
move them.

What Do You Triage
------------------

As a triage owner the queries you should be following for your component
are:

-  All open bugs, in your components without a pending ``needinfo`` flag
   which do not have a valid value of severity set
-  All bugs with active review requests in your component which have not
   been modified in five days
-  All bugs with reviewed, but unlanded patches in your components
-  All bugs with a needinfo request unanswered for more than 10 days

There’s a tool with these queries to help you find bugs
https://mozilla.github.io/triage-center/ and the source is at
https://github.com/mozilla/triage-center/.

If a bug is an enhancement it needs a priority set and a target release
or program milestone. These bugs are normally reviewed by product
managers. Enhancements can lead to release notes and QA needed that we
also need to know about

If a bug is a task resulting in a changeset, release managers will need
to known when this work will be done. A task such as refactoring fragile
code can be risky.

Weekly or More Frequently (depending on the component) find un-triaged
bugs in the components you triage.

Decide the :ref:`Severity <Defect Severity>`  for each untriaged bug
(you can override what’s already been set.)

These bugs are reviewed in the weekly Regression Triage meeting

- Bugs of type ``defect`` with the ``regression`` keyword without
  ``status-firefoxNN`` decisions
- Bugs of type ``defect`` with the ``regression`` keyword without
  a regression range

Automatic Bug Updates
~~~~~~~~~~~~~~~~~~~~~

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

Assumptions
~~~~~~~~~~~

If a bug's release status in Firefox version N was ``affected`` or ``wontfix``,
its Severity is ``S3`` or ``S4`` and its Priority is ``P3`` or lower (backlog,)
then its release status in Firefox version N+1, if the bug is still open,
is considered to be ``wontfix``.

Questions and Edge Cases
------------------------

This bug is a feature request
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the bug’s type to ``enhancement``, add the ``feature`` keyword if
relevant, and state to ``NEW``. Set the bug's Severity to ``N/A``. This
bug will be excluded from future triage queries.

This bug is a task, not a defect
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the bug’s type to ``task``, and state to ``NEW``. Set the bug's
Severity to ``N/A``. This bug will be excluded from future triage queries.


If you are not sure of a bug’s type, check :ref:`our rules for bug
types <Bug Types>`.

This bug’s state is ``UNCONFIRMED``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Are there steps to reproduce? If not, needinfo the person who filed the
bug, requesting steps to reproduce. You are not obligated to wait
forever for a response, and bugs for which open requests for information
go unanswered can be ``RESOLVED`` as ``INCOMPLETE``.

I need help reproducing the bug
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set a needinfo for the QA managers, Softvision project managers, or the
QA owner of the component of the bug.

I don’t have enough information to make a decision
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you don’t have a reproduction or confirmation, or have questions
about how to proceed, ``needinfo`` the person who filed the bug, or
someone who can answer.

The ``stalled`` keyword
~~~~~~~~~~~~~~~~~~~~~~~

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

Bug is in the wrong Component
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the bug has a Severity of ``S3``, ``S4``, or ``N/A`` move the what
you think is the correct component, or needinfo the person
responsible for the component to ask them.

If the bug has a Severity of ``S1`` or ``S2`` then notify Release Management
and contact the triage owner of the component for which you think it belongs to.
We cannot lose track of a high severity bug because it is in the wrong component.

My project is on GitHub
~~~~~~~~~~~~~~~~~~~~~~~

We have :ref:`a guide for GitHub projects to follow <GitHub Metadata Recommendations>` when
triaging. (Note: this guide needs updating.)

Summary
-------

Multiple times weekly
~~~~~~~~~~~~~~~~~~~~~

Use queries for the components you are responsible for in
https://mozilla.github.io/triage-center/ to find bugs in
need of triage.

For each untriaged bug:

-  Assign a Severity
-  **Do not** assign a ``defect`` a Severity of
   ``N/A``

You can, but are not required to set the bug's :ref:`Priority <Priority Definitions>`.

Watch open needinfo flags
~~~~~~~~~~~~~~~~~~~~~~~~~

Don’t let open needinfo flags linger for more than two weeks.

Close minor bugs with unresponded needinfo flags.

Follow up on needinfo flag requests.

The `Triage Center tool <https://mozilla.github.io/triage-center/>`__ will help you find these.

End of Iteration/Release Cycle
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Any open ``S1`` or ``S2`` bugs at the end of the release cycle
will require review by engineering and release management. A
policy on this is forthcoming.

Optional
^^^^^^^^

(The guidelines on bug priority are under review.)

Are there open P1s? Revisit their priority,
and move to them to the backlog (``P3``) or ``P2``.

Are there ``P2`` bugs that should move to ``P1``
for the next cycle?

Are there ``P2`` you now know are lower priority,
move to ``P3``.

Are there ``P3`` bugs you now know you won’t get to?
Either demote to ``P5`` (will accept patch) or
resolve as ``WONTFIX``.

Getting help
------------

-  Ask in #bug-handling on chat.mozilla.org
