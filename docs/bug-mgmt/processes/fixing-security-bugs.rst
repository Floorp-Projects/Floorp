Fixing Security Bugs
====================

A bug has been reported as security-sensitive in Bugzilla and received a
security rating.

If this bug is private - which is most likely for a reported security
bug - **the process for patching is slightly different than the usual
process for fixing a bug**.

Here are security guidelines to follow if you’re involved in reviewing,
testing and landing a security patch. See
:ref:`Security Bug Approval Process`
for more details about how to request sec-approval and land the patch.

Keeping private information private
-----------------------------------

A security-sensitive bug in Bugzilla means that all information about
the bug except its ID number are hidden. This includes the title,
comments, reporter, assignee and CC’d people.

A security-sensitive bug usually remains private until a fix is shipped
in a new release, **and after a certain amount of time to ensure that a
maximum number of users updated their version of Firefox**. Bugs are
usually made public after 6 months and a couple of releases.

From the moment a security bug has been privately reported to the moment
a fix is shipped and the bug is set public, all information about that
bug need to be handled carefully in order to avoid an unmitigated
vulnerability to be known and exploited out there before we release a
fix (0-day).

During a normal process, information about the nature of bug can be
accessed through:

-  Bug comments (Bugzilla, GitHub issue)
-  Commit message (visible on Bugzilla, tree check-ins and test servers)
-  Code comments
-  Test cases
-  Bug content can potentially be discussed on public IRC/Slack channels
   and mailing list emails.

When patching for a security bug, you’ll need to be mindful about what
type of information you share and where.

In commit messages
~~~~~~~~~~~~~~~~~~

People are watching code check-ins, so we want to avoid sharing
information which would disclose or help finding a vulnerability too
easily before we shipped the fix to our users. This includes:

-  The **nature of the vulnerability** (overflow, use-after-free, XSS,
   CSP bypass...)
-  **Ways to trigger and exploit that vulnerability**
   - In commit messages, code comments and test cases.
-  The fact that a bug / commit is security-related:

   -  **Trigger words** in the commit message or code comments such as "security", "exploitable" or the nature of a security vulnerability (overflow, use-after-free…)
   -  **Security approver’s name** in a commit message.
-  The Firefox versions and components affected by the vulnerability.
-  Patches with an obvious fix.

In Bugzilla and other public channels
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to commits, you’ll need to be mindful of not disclosing
sensitive information about the bug in public places, such as Bugzilla:

-  **Do not add public bugs in the “duplicate”, “depends on”, “blocks”
   or “see also” section if these bugs could give hints about the nature
   of the security issue.**

   -  Mention the bugs in comment of the private bug instead.
-  Do not comment sensitive information in public related bugs.
-  Also be careful about who you give bug access to: **double check
   before CC’ing the wrong person or alias**.

On IRC, Slack channels, GitHub issues, mailing lists: If you need to
discuss about a security bug, use a private channel (protected with a
password or with proper right access management)

During Development
------------------

Testing sec-high and sec-critical bugs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pushing to Try servers requires Level 1 Commit access but **content
viewing is publicly accessible**.

As much as possible, **do not push to Try servers**. Testing should be
done locally before checkin in order to prevent public disclosing of the
bug.

If you need to push to Try servers, make sure your tests don’t disclose
what the vulnerability is about or how to trigger it. Do not mention
anywhere it is security related.

Obfuscating a security patch
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If your security patch looks obvious because of the code it contains
(e.g. a one-line fix), or if you really need to push to Try servers,
**consider integrating your security-related patch to non-security work
in the same area**. And/or pretend it is related to something else, like
some performance improvement or a correctness fix. **Definitely don't
include the bug number in the commit message.** This will help making
the security issue less easily identifiable. (The absolute ban against
"Security through Obscurity" is in relation to cryptographic systems. In
other situations you still can't *rely* on obscurity but it can
sometimes buy you a little time. In this context we need to get the
fixes into the hands of our users faster than attackers can weaponize
and deploy attacks and a little extra time can help.)

Requesting sec-approval
~~~~~~~~~~~~~~~~~~~~~~~

See :ref:`Security Bug Approval Process`
for more details

Landing tests
~~~~~~~~~~~~~

Tests can be landed **after the release has gone live** and **not before
at least 4 weeks following that release**.

The exception is if a security issue has never been shipped in a release
build and has been fixed in all development branches.

Making a security bug public
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is the responsibility of the security management team.

Essentials
----------

-  **Do not disclose any information about the vulnerability before a
   release with a fix has gone live for enough time for users to update
   their software**.

   -  This includes code comments, commit messages, tests, public
      communication channels.

-  If any doubt: '''request sec-approval? '''
-  If any doubt: **needinfo security folks**.
-  **If there’s no rating, assume the worst and treat the bug as
   sec-critical**.

Documentation & Contacts
------------------------

- `Normal process for submitting a patch <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/How_to_Submit_a_Patch>`__
- `How to file a security bug <https://wiki.mozilla.org/Security/Fileabug>`__
- `Handling Mozilla security bugs (policy) <https://www.mozilla.org/en-US/about/governance/policies/security-group/bugs/>`__
- `Security Bug Approval Process <security-approval>`__
- `Contacting the Security team(s) at Mozilla: <https://wiki.mozilla.org/Security>`__
