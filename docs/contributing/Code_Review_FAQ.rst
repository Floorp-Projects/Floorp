Code Review FAQ
===============

What is the purpose of code review?
-----------------------------------

Code review is our basic mechanism for validating the design and
implementation of patches. It also helps us maintain a level of
consistency in design and implementation practices across the many
hackers and among the various modules of Mozilla.

Of course, code review doesn't happen instantaneously, and so there is
some latency built into the system. We're always looking for ways to
reduce the wait, while simultaneously allowing reviewers to do a good
chunk of hacking themselves. We don't have a perfect system, and we
never will. It's still evolving, so let us know if you have suggestions.

Mozilla used to have the concept of "super-review", but `a consensus was
reached in
2018 <https://groups.google.com/forum/#!topic/mozilla.governance/HHU0h-44NDo>`__
to retire this process.

Who must review my code?
------------------------

You must have an approval ("r={{ mediawiki.external('name') }}") from
the module owner or designated "peer" of the module where the code will
be checked in. If your code affects several modules, then generally you
should have an "r={{ mediawiki.external('name') }}" from the owner or
designated peer of each affected module. We try to be reasonable here,
so we don't have an absolute rule on when every module owner must
approve. For example, tree-wide changes such as a change to a string
class or a change to text that is displayed in many modules generally
doesn't get reviewed by every module owner.

You may wish to ask others as well.


What do reviewers look for?
---------------------------

A review is focused on a patch's design, implementation, usefulness in
fixing a stated problem, and fit within its module. A reviewer should be
someone with domain expertise in the problem area. A reviewer may also
utilize other areas of his or her expertise and comment on other
possible improvements. There are no inherent limitations on what
comments a reviewer might make about improving the code.

Reviewers will probably look at the following areas of the code:

-  “goal” review: is the issue being fixed actually a bug? Does the
   patch fix the fundamental problem?
-  API/design review. Because APIs define the interactions between
   modules, they need special care. Review is especially important to
   keep APIs balanced and targeted, and not too specific or
   overdesigned. There are a `WebIDL review
   checklist <https://wiki.mozilla.org/WebAPI/WebIDL_Review_Checklist>`__.
   There are also templates for emails that should be sent when APIs are
   going to be exposed to the Web and general guidance around naming on
   `this wiki
   page <https://wiki.mozilla.org/WebAPI/ExposureGuidelines>`__.
-  Maintainability review. Code which is unreadable is impossible to
   maintain. If the reviewer has to ask questions about the purpose of a
   piece of code, then it is probably not documented well enough. Does
   the code follow the :ref:`Coding style` ? Be careful when
   reviewing code using modern C++ features like auto.
-  Security review. Does the design use security concepts such as input
   sanitizers, wrappers, and other techniques? Does this code need
   additional security testing such as fuzz-testing or static analysis?
-  Integration review. Does this code work properly with other modules?
   Is it localized properly? Does it have server dependencies? Does it
   have user documentation?
-  Testing review. Are there tests for correct function? Are there tests
   for error conditions and incorrect inputs which could happen during
   operation?
-  Performance review. Has this code been profiled? Are you sure it's
   not negatively affecting performance of other code?
-  License review. Does the code follow the `code licensing
   rules <http://www.mozilla.org/hacking/committer/committers-agreement.pdf>`__?


How can I tell the status of reviews?
-------------------------------------

When a patch has passed review you'll see "Accepted" in green at the top
of a Phabricator revision, under the title. In Bugzilla (which is
deprecated in favour of Phabricator), this is indicated by "{{
mediawiki.external('name') }}:review+" in the attachment table in the
bug report. If it has failed review then you'll see "Needs Revision" in
red at the top of the revision, or, in Bugzilla, "{{
mediawiki.external('name') }}:review-". Most of the time that a reviewer
sets a review flag, they will also add a comment to the bug explaining
the review.
