Feature flags
=============

.. raw:: html

   <p style="color: white; background-color: red;">

**Note**\ *: this policy is obsolete and will be replaced by a unified
feature tracking policy under development.*

.. raw:: html

   </p>

Product and release need to track bugs whose visibility is controlled
through a pref. Once a feature has been QAed and approved for a release,
the preference should be enabled.

Policy
~~~~~~

*For any feature or fix controlled by a preference flag in Firefox,
there should be a single bug (e.g. a meta bug) to track its release. If
the feature requires multiple bugs/patches then this should be a
``meta`` bug.*

*The bug which tracks a feature or fix that is controlled by a flag in
Firefox preferences must do the following:*

*It*\ **must**\ *use the ``behind-pref`` flag. The leads for the feature
would need to update the flags appropriately until the bug is closed.*

*It*\ **should**\ *be added to the Firefox Feature Trello board.*

*It*\ **must**\ *state in a comment or the summary which preference will
be used to manage visibility.*

*It*\ **must**\ *request the ``qe-verify`` flag (setting it to ``?``)
and the bug*\ **must**\ *be verfified (Status: RESOLVED, Resolution:
VERIFIED) by QA once they have accepted the ``qe-verify`` request
(setting the flag to ``+``) before the feature can be promoted to Beta.*

*The severity of the bug tracking the feature*\ **must**\ *be set to
``enhancement``.*

*The Release Managers for each train must consent to the feature being
enabled on that train.*

*QA*\ **must**\ *sign off on the feature before it is enabled in Beta.*

*Any released feature*\ **must**\ *have a value of the parent bug’s
``behind-pref`` flag set to the version where the feature was released,
and have a state of VERIFIED and resolution FIXED.*

The ``behind-pref`` flag
~~~~~~~~~~~~~~~~~~~~~~~~

The ``behind-pref`` flag is a multi-valued release-status flag with the
values

-  ``---``
-  ``in-progress``
-  ``off``
-  ``releaseNN``
-  ``betaNN+1``
-  ``nightlyNN+2``

Where NN is the current release version of Firefox.

Values and Meanings
^^^^^^^^^^^^^^^^^^^

.. raw:: html

   <dl>

.. raw:: html

   <dt>

—

.. raw:: html

   </dt>

.. raw:: html

   <dd>

This is not a feature that is preffed-off

.. raw:: html

   </dd>

.. raw:: html

   <dt>

in-progress

.. raw:: html

   </dt>

.. raw:: html

   <dd>

One or more bugs implementing the feature are still in progress and the
feature is not available in any release

.. raw:: html

   </dd>

.. raw:: html

   <dt>

off

.. raw:: html

   </dt>

.. raw:: html

   <dd>

The code for this feature has landed in m-c but the feature is
preffed-off in all releases

.. raw:: html

   </dd>

.. raw:: html

   <dt>

releaseNN

.. raw:: html

   </dt>

.. raw:: html

   <dd>

Feature was enabled in or will ride the trains to Release NN

.. raw:: html

   </dd>

.. raw:: html

   <dt>

betaNN

.. raw:: html

   </dt>

.. raw:: html

   <dd>

The feature was enabled in Beta NN and Nightly but not riding train to
Release

.. raw:: html

   </dd>

.. raw:: html

   <dt>

nightlyNN

.. raw:: html

   </dt>

.. raw:: html

   <dd>

The feature was enabled in Nightly NN only

.. raw:: html

   </dd>

.. raw:: html

   </dl>

Maintenance
^^^^^^^^^^^

If, as of release version 60, current values of the flag were:

-  ``release60``
-  ``release61``
-  ``release62``
-  ``beta61``
-  ``beta62``
-  ``nightly62``

on merge day we would add

-  ``release63``
-  ``beta63``
-  ``nightly63``

and disable (but not delete) ``release60``, ``beta61``, and
``nightly62``.

ESR
^^^

For tracking the feature in ESR, we create a ``behind-pref-esr`` status
flag. It will be kept up with the values of the current, previous, and
next ESR releases.

*Example*

-  ``---``
-  ``off``
-  ``esr52``
-  ``esr60``
-  ``esr72``

Example
~~~~~~~

A bug is filed, “Make Tabby Cats the default new tab experience.” And
the team developing this (engineering and product) decide that this
should be controlled behind a preference,
``browser.newtabpage.default.tabbycat``. The developers break the work
for this feature down into three bugs. A fourth bug will be used to
track the preference flag.

-  The bug’s summary is updated to
   ``[meta] Make Tabby Cats the default new tab experience``
-  A comment is filed listing the name of the preference
-  The ``behind-pref`` flag is set to ``in-progress``
-  The bug’s severity is set to ``enhancement``
-  The three implementation bugs and the pref bug should be marked as
   blocking the ``[meta]`` bug for the new feature

As the feature is developed and the individual patches implement it
land, it’s kept off by compiler directives, the pref, or both. As these
land, and are not backed out, these bugs can be marked RESOLVED FIXED.

The lead for the feature–which may be an engineer, a program manager, or
a product manager–must notify the Nightly Release Manager before
enabling it.

-  The bug’s ``behind-pref`` flag is set to ``nightlyNN`` where NN is
   the current version of nightly to indicate it’s now available in
   nightly
-  The ``qe-verify`` flag is set to +, requesting QA’s attention

Before the feature can graduate to Beta, it must be verified by QA.

-  The feature is tested on nightly and confirmed to work as specified
   (implicit here is the feature team’s involvement in creating a test
   plan)

If the feature does not pass testing then QA should file bugs blocking
the ``[meta]`` bug for the feature. QA and the development team must
confer and decide if the feature will be disabled in Nightly, or allowed
to be kept on while bugs are fixed. This will depend on risk and
severity of the bugs found.

If it’s decided to disable the feature, then it should be turned off in
the nightly build and the ``behind-pref`` flag set to ``off``. The bug’s
comments should explain how that decision was reached. Once the defects
have been resolved, then ``behind-pref`` can be reset to ``nightlyNN``.

Once the feature has been verfied by QA then:

-  The bug should be enabled in Beta once Release Management approves
-  the ``behind-pref`` flag is updated to ``releaseNN`` where NN is the
   next release.

Once the patch for the bug to enable in Beta lands:

-  QA moves the bug’s status to VERIFIED and resolution to FIXED

The feature now *rides the trains* to release. The bug is then
considered completed.

If it’s decided to hold the feature out of the next release and let Beta
users try it out, then the ``behind-pref`` flag is set to ``betaNN``
where NN is the next beta. Once the decision is made to let the feature
ride the trains, then it is updated to ``releaseNN`` where NN is the
target release.

When the feature is merged to ESR the ``behind-pref-esr`` field should
be set to the version where it will be released.

Questions
~~~~~~~~~

What if we turn off the feature in the main release?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The main bug’s ``behind-pref`` value should be reset to the releases
it’s still on, ``betaNN+1`` or ``nightlyNN+2``; or to ``off``, and the
bug’s status set to REOPENED.

The bug to turn off the feature must be a dependency of the main bug.

What about bugs found in a feature after release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These bugs do not need the ``behind-pref`` flag. If it’s decided that
the feature should be turned off until the bug or bugs are fixed, then
these bugs should block the original feature tracking bug.

What if we want to hold a feature over a release cycle and not promote it?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

On merge day, the ``behind-pref`` flag would retain it’s earlier value,
and remain preffed off in other versions.

What if I want to enable parts of my feature in Nightly?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If your feature is incomplete, but some functionality is available, then
mark ``behind-pref`` as ``nightlyNN`` where NN is the current nighty
version. Do not request ``qe-verify`` until the feature is complete.

If you plan to incrementally add functionality to Nightly over a number
of release cycles, then you can use a single ``meta`` bug to keep track
of functionality, but don’t promote the feature to ``Beta``.

If you intend to implement functionality over a number of Beta and
Release cycles, then the tracking/meta bug should not be marked as FIXED
VERIFIED until the feature is completed.

What about gradual rollout of features
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you intend to roll out the feature gradually, then the rollout should
be tracked in the feature bug’s comments. If the the rollout percentage
is controlled by a preference, then changes to that preference should be
blockers of the the feature bug.

Tracking queries
~~~~~~~~~~~~~~~~

-  Open bugs for features behind preferences
-  Open bugs for features behind preferences landed but not QAed
-  Bugs for features in upcoming release
-  Bugs for features which have been disabled
