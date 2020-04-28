Security Bug Approval Process
=============================

How to fix a core-security bug in Firefox - developer guidelines
----------------------------------------------------------------

Follow these security guidelines if you’re involved in reviewing,
testing and landing a security patch:
:ref:`Fixing Security Bugs`.

Purpose: don't 0-day ourselves
------------------------------

People watch our check-ins. They may be able to start exploiting our
users before we can get an update out to them if

-  the patch is an obvious security fix (bounds check, kungFuDeathGrip,
   etc.)
-  the check-in comment says "security fix" or includes trigger words
   like "exploitable", "vulnerable", "overflow", "injection", "use after
   free", etc.
-  comments in the code mention those types of things or how someone
   could abuse the bug
-  the check-in contains testcases that show exactly how to trigger the
   vulnerability

Principle: assume the worst
---------------------------

-  If there's no rating we assume it could be critical
-  If we don't know the regression range we assume it needs porting to
   all supported branches

Process for Security Bugs (Developer Perspective)
-------------------------------------------------

Filing / Managing Bugs
~~~~~~~~~~~~~~~~~~~~~~

-  Try whenever possible to file security bugs marked as such when
   filing, instead of filing them as open bugs and then closing later.
   This is not always possible, but attention to this, especially when
   filing from crash-stats, is helpful.
-  Avoid linking it to non-security bugs with Blocks, Depends,
   Regressions, or See Also, especially if those bugs may give a hint to
   the sort of security issue involved. Mention the bug in a comment on
   the security bug instead. We can always fill in the links later after
   the fix has shipped.

Developing the Patch
~~~~~~~~~~~~~~~~~~~~

-  Comments in the code should not mention a security issue is being
   fixed. Don’t paint a picture or an arrow pointing to security issues
   any more than the code changes already do.
-  Avoid linking it to non-security bugs with Blocks, Depends, or See
   Also, especially if those bugs may give a hint to the sort of
   security issue involved. Mention the bug in a comment on the security
   bug instead. We can always fill in the links later after the fix has
   shipped.
-  Do not push to Try servers if possible: this exposes the security
   issues for these critical and high rated bugs to public viewing. In
   an ideal case, testing of patches is done locally before final
   check-in to mozilla-central.
-  If pushing to Try servers is necessary, **do not include the bug
   number in the patch**. Ideally, do not include tests in the push as
   the tests can illustrate the exact nature of the security problem
   frequently.
-  If you must push to Try servers, with or without tests, try to
   obfuscate what this patch is for. Try to push it with other,
   non-security work, in the same area.

Request review of the patch in the same process as normal. After the
patch has received an r+ you will request sec-approval. See
:ref:`Fixing Security Bugs`
for more examples/details of these points.

On Requesting sec-approval
~~~~~~~~~~~~~~~~~~~~~~~~~~

For security bugs with no sec- severity rating assume the worst and
follow the rules for sec-critical. During the sec-approval process we
will notice it has not been rated and rate it during the process.

Core-security bug fixes can be landed by a developer without any
explicit approval if:

| **A)** The bug has a sec-low, sec-moderate, sec-other, or sec-want
  rating.
|    **or**
| **B)** The bug is a recent regression on mozilla-central. This means

-  A specific regressing check-in has been identified
-  The developer can (**and has**) marked the status flags for ESR,
   Beta, and Aurora as "unaffected"
-  We have not shipped this vulnerability in anything other than a
   nightly build

If it meets the above criteria, check that patch in.

Otherwise, if the bug has a patch \*and\* is sec-high or sec-critical,
the developer should prepare the patch for sec-approval. This entails:

-  Commit should occur without specific mention of security, security
   bugs, or sec-approvers if possible. While comprehensive commit
   messages are generally encouraged; they should be omitted for
   security bugs and instead be posted in the bug (which will eventually
   become public.)
-  Separate out tests into a separate commit. **Do not commit tests when
   checking in** when the security bug fix is initially checked-in.
   **Remember we don’t want to 0-day ourselves!**

   -  Tests should only be checked in later, after an official Firefox
      release that contains the fix has gone live and not for at least
      four weeks following that release. For example, if Firefox 53
      contains a fix for a security issue that affects the world and is
      then fixed in 54, tests for this fix should not be checked in
      until four weeks after 54 goes live. The exception to this is if
      there is a security issue that hasn’t shipped in a release build
      and it is being fixed on multiple development branches (such as
      mozilla-central and beta). Since the security problem was never
      released to the world, once the bug is fixed in all affected
      places, tests can be checked in to the various branches.
   -  There are two main techniques for remembering to check in the
      tests later:

      -  clone the sec bug into a hidden "task" bug "land tests for bug
         xxxxx" and assign to yourself. It should get a "sec-other"
         keyword rating.
      -  Or, set the "in-testsuite" flag to "?", and later set it to "+"
         when the tests get checked in.

Following that, set the sec-approval flag to '?' on the patch when it is
ready to be checked into mozilla-central (or elsewhere if it is branch
only).

If developers are unsure about a bug and it has a patch ready, just
request sec-approval anyway and move on. Don't overthink it!

An automatic nomination comment will be added to bugzilla when
sec-approval is set to '?'. The questions in this need to be filled out
as best as possible when sec-approval is requested for the patch.

It is as follows (courtesy of Dan Veditz)::

   [Security approval request comment]
   How easily can the security issue be deduced from the patch?
   Do comments in the patch, the check-in comment, or tests included in
   the patch paint a bulls-eye on the security problem?
   Which older supported branches are affected by this flaw?
   If not all supported branches, which bug introduced the flaw?
   Do you have backports for the affected branches? If not, how
   different, hard to create, and risky will they be?
   How likely is this patch to cause regressions; how much testing does
   it need?

This is similar to the ESR approval nomination form and is meant to help
us evaluate the risks around approving the patch for checkin.

When the bug is approved for landing, the sec-approval flag will be set
to '+' with a comment from the approver to land the patch. At that
point, land it according to instructions provided..

This will allow us to control when we can land security bugs without
exposing them too early and to make sure they get landed on the various
branches.

If you have any questions or are unsure about anything in this document
contact us on Slack in the #security channel or the current
sec-approvers Dan Veditz and Tom Ritter.

Process for Security Bugs (sec-approver Perspective)
----------------------------------------------------

The security assurance team and release management will have their own
process for approving bugs:

#. The Security assurance team goes through sec-approval ? bugs daily
   and approves low risk fixes for central (if early in cycle).
   Developers can also ping the Security Assurance Team (specifically
   Tom Ritter & Dan Veditz) in #security on Slack when important.

   #. If a bug lacks a security-rating one should be assigned - possibly
      in coordination with the (other member of) the Security Assurance
      Team

#. Security team marks tracking flags to ? for all affected versions
   when approved for central. (This allows release management to decide
   whether to uplift to branches just like always.)
#. Weekly security/release management triage meeting goes through
   sec-approval + and ? bugs where beta and ESR is affected, ? bugs with
   higher risk (sec-high and sec-critical), or ? bugs near end of cycle.

Options for sec-approval including a logical combination of the
following:

-  Separate out the test and comments in the code into a followup commit
   we will commit later.
-  Remove the commit message and place it in the bug or comments in a
   followup commit.
-  Please land it bundled in with another commit
-  Land today
-  Land today, land the tests after
-  Land closer to the release date
-  Land in Nightly to assess stability
-  Land today and request uplift to all branches
-  Request uplift to all branches and we'll land as close to shipping as
   permitted
-  Chemspill time

The decision process for which of these to choose is perceived risk on
multiple axes:

-  ease of exploitation
-  reverse engineering risk
-  stability risk

The most common choice is: not much stability risk, not an immediate RE
risk, moderate to high difficulty of exploitation: "land whenever"