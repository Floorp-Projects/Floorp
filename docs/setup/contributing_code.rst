How To Contribute Code To Firefox
=================================

The whole process can be a bit long, and it might take time to get things right.
If at any point you are stuck, please don't hesitate to ask at `https://chat.mozilla.org <https://chat.mozilla.org>`_
in the `#introduction <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`_ channel.

We make changes to Firefox by writing patches, testing them and pushing them into "the tree", the
term we use for all the code in Mozilla-Central. Let's get started.

Please see the :ref:`Firefox Contributors Quick Reference <Firefox Contributors' Quick Reference>` for simple check list.

Finding something to work on
----------------------------

| Bugs listed as 'Assigned' are not usually a good place to start,
  unless you're sure you have something worthy to contribute. Someone
  else is already working on it!
| Even with no assignee, it is polite to check if someone has recently
  commented that they're looking at fixing the issue.
| Once you have found something to work on, go ahead and comment! Let
  the bug submitter, reviewer, and component owner know that you'd like
  to work on the bug. You might receive some extra information, perhaps
  also made the assignee.

Find a bug we've identified as a good fit for new contributors.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With more than a million bugs filed in Bugzilla, it can be hard to know
where to start, so we've created these bug categories to make getting
involved a little easier:

-  `Codetribute <https://codetribute.mozilla.org/>`_ - our site for
   finding bugs that are mentored, some are good first bugs, some are
   slightly harder. Your mentor will help guide you with the bug fix and
   through the submission and landing process.
-  `Good First Bugs <https://mzl.la/2yBg3zB>`_
   - are the best way to take your first steps into the Mozilla
   ecosystem. They're all about small changes, sometimes as little as a
   few lines, but they're a great way to learn about setting up your
   development environment, navigating Bugzilla, and making
   contributions to the Mozilla codebase.
-  `Student Projects <https://bugzil.la/kw:student-project>`_ - are
   larger projects, such as might be suitable for a university student
   for credit. Of course, if you are not a student, feel free to fix one
   of these bugs. We maintain two lists: one for projects `based on the
   existing codebase <https://bugzil.la/kw:student-project>`_.

Fix that one bug
~~~~~~~~~~~~~~~~

If there's one particular bug you'd like to fix about Firefox, Thunderbird, or
your other favorite Mozilla application, this can be a great place to
start. There are a number of ways to do this:

-  `Search bugzilla <https://bugzilla.mozilla.org/query.cgi>`_ for
   relevant keywords. See pages on
   `Bugzilla <https://developer.mozilla.org/docs/Mozilla/Bugzilla>`_ and `Searching
   Bugzilla <https://developer.mozilla.org/docs/Mozilla/QA/Searching_Bugzilla>`_ for further
   help
-  Learn the `bugzilla
   component <https://bugzilla.mozilla.org/describecomponents.cgi>`_,
   with which your pet bug is implemented, using the components list.
   Browse this component on bugzilla for related bugs

Fixing your bug
---------------

We leave this in your hands. Here are some further resources to help:

-  Check out
   `https://developer.mozilla.org/docs/Developer_Guide <https://developer.mozilla.org/docs/Developer_Guide>`_
   and its parent document,
   https://developer.mozilla.org/docs/Mozilla
-  Our :ref:`reviewer checklist <Reviewer_Checklist>` is very
   useful, if you have a patch near completion, and seek a favorable
   review
-  Utilize our build tool :ref:`mach`, its linting,
   static analysis, and other code checking features

Getting your code reviewed
--------------------------

Once you fix the bug, you can advance to having your code reviewed.

Mozilla uses
`Phabricator <https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html>`_
for code review.

Who is the right person to ask for a review?

-  If you have a mentored bug: ask your mentor. They will help, or can
   easily find out. It might be them!
-  Run ``hg blame`` on the file and look for the people who have touched
   the functions you're working on. They too are good candidates.
   Running ``hg log`` and looking for regular reviewers might be a
   solution too.
-  The bug itself may contain a clear indication of the best person to
   ask for a review
-  Are there related bugs on similar topics? The reviewer in those bugs
   might be another good choice
-  We have an out of date `list of
   modules <https://wiki.mozilla.org/Modules>`_, which lists peers and
   owners for the module. Some of these will be good reviewers. In a
   worst case scenario, set the module owner as the reviewer, asking
   them in the comments to pick someone more suitable

Please select only one reviewer.

Following up and responding
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once you've asked for a review, a reviewer will often respond within a
day or two, reviewing the patch, or saying when they will be able to
review it, perhaps due to a backlog. If you don't hear back within this
time, naturally reach out to them: add a comment to the bug saying
'review ping?', check the "Need more information from" box, and add the
reviewer's name. If they don't respond within a day or two, you can ask
for help on Matrix in the
`#introduction:mozilla.org <https://riot.im/app/#/room/#introduction:mozilla.org>`_
or
`#developers:mozilla.org <https://chat.mozilla.org/#/room/#developers:mozilla.org>`_
channels, or contact `Mike
Hoye <mailto:mhoye@mozilla.com?subject=Code%20Review%20Request%20&body=URL%3A%20%20%5Bplease%20paste%20a%20link%20to%20your%20patch%20here.%5D>`_
directly.

Don't hesitate to contact your mentor as well if this isn't moving.

For most new contributors, and even for long-time Mozillians, the first
review of your patch will be "Requested Changes" (or an "r-" in
Bugzilla). This does not mean you've done bad work. There is more work
to do before the code can be merged into the tree. Your patch may need
some changes - perhaps minor, perhaps major - and your reviewer will
give you some guidance on what needs to be done next.

This is an important process, so don't be discouraged! With our
long-lived codebase, and hundreds of millions of users, the care and
attention helping contributors bring good patches is the cornerstone of
the Mozilla project. Make any changes your reviewer seeks; if you're
unsure how, be sure to ask! Push your new patch up to Phabricator again and
ask for a further review from the same reviewer. If they accept your
changes, this means your patch can be landed into the tree!

Getting code into Firefox
-------------------------

Once your patch has been accepted, it is ready to go. Before it can be
merged into the tree, your patch will need to complete a successful run
through our `try
server <https://wiki.mozilla.org/ReleaseEngineering/TryServer>`_,
making sure there are no unexpected regressions. If you don't have try
server access already, your mentor, or the person who reviewed your
patch, will be able to help.

Ask the reviewer to land the patch for you.
For more details, see :ref:`push_a_change`


Do it all again!
----------------

Thank you. You've fixed your very first bug, and the Open Web is
stronger for it. But don't stop now.

Go back to step 3, as there is plenty more to do. Your mentor might
suggest a new bug for you to work on, or `find one that interests
you <http://www.whatcanidoformozilla.org/>`_. Now that you've got your
first bug fixed you should request level 1 access to the repository to
push to the try server and get automated feedback about your changes on
multiple platforms. After fixing a nontrivial number of bugs you should
request level 3 access so you can land your own code after it has been
reviewed.

More information
----------------

We're in the process of improving information on this page for newcomers
to the project. We'll be integrating some information from these pages
soon, but until then you may find them interesting in their current
form:

-  `A guide to learning the Firefox
   codebase <http://www.joshmatthews.net/blog/2010/03/getting-involve-with-mozilla/>`_
-  `A beginner's guide to SpiderMonkey, Mozilla's Javascript
   engine <https://wiki.mozilla.org/JavaScript:New_to_SpiderMonkey>`_
-  `Mozilla platform development
   cheatsheet <https://web.archive.org/web/20160813112326/http://www.codefirefox.com:80/cheatsheet>`_
   (archive.org)
