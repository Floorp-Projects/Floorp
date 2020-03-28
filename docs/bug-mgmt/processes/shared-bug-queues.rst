Shared Bug Queues
=================

Reviewers for change sets can be suggested at the product and component
level, but only the person who has been asked to review code will be
notified.

Realizing that Bugzilla users can *watch* other users, `Chris
Cooper <https://mozillians.org/en-US/u/coop/>`__ came up with the idea
of having `a shared reviews alias for review
requests <http://coopcoopbware.tumblr.com/post/170952242320/experiments-in-productivity-the-shared-bug-queue>`__.

If you want to watch a particular part of the tree in Mozilla Central,
then `use the Herald
tool <https://phabricator.services.mozilla.com/book/phabricator/article/herald/>`__.

Process
-------

1. Create a new bugzilla.mozilla.com account for an address which can
   receive mail.

-  Use the ``name+extension@domain.tld`` trick such as
   ``jmozillian+reviews@mozilla.com`` to create a unique address
2. Respond to the email sent by Bugzilla and set a password on the
   account
3. `Open a bug <https://mzl.la/2Mg8Sli>`__ to convert the account to a
   bot and make it the shared review queue for your component
4. BMO administrator updates the email address of the new account to the
   ``@mozilla.bugs`` address
5. BMO administrator updates the default reviewer for the component
   requested and sets it to the shared review account
6. Reviewers `follow the shared review account in
   bugzilla <https://bugzilla.mozilla.org/userprefs.cgi?tab=email>`__
7. Reviewers get notified when shared review account is ``r?``\ ed
