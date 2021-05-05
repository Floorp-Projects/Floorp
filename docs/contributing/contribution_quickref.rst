Firefox Contributors' Quick Reference
=====================================

Some parts of this process, including cloning and compiling, can take a long time even on modern hardware.
If at any point you get stuck, please don't hesitate to ask at `https://chat.mozilla.org <https://chat.mozilla.org>`__
in the `#introduction <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`__ channel.

Before you start
----------------
Please register and create your account for

`Bugzilla <https://bugzilla.mozilla.org/>`__ : web-based general-purpose bug tracking system.
To register with Phabricator, make sure you enable Two-Factor Authentication (My Profile >> Edit Profile & Preferences >> Two-Factor Authentication) in Bugzilla.

`Phabricator <https://phabricator.services.mozilla.com/>`__: web-based software development collaboration tools, mainly for code review.
Please obtain an API Token (Settings >> Conduit API Tokens)

Clone the sources
-----------------

You can use either mercurial or git. `Mercurial <https://www.mercurial-scm.org/downloads>`__ is the canonical version control system.

.. code-block:: shell

    $ hg clone https://hg.mozilla.org/mozilla-central/

For git, see the `git cinnabar documentation <https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-git-workflow-for-Gecko-development>`__

The clone can take from 40 minutes to two hours (depending on your connection) and
the repository should be less than 5GB (~ 20GB after the build).

If you have any network connection issues and cannot clone with command, try :ref:`Mercurial bundles <Mercurial bundles>`.

:ref:`More information <Mercurial Overview>`

Install dependencies (non-Windows)
----------------------------------

Firefox provides a mechanism to install all dependencies; in the source tree:

.. code-block:: shell

     $ ./mach bootstrap

The default options are recommended.
If you're not planning to write C++ or Rust code, select :ref:`Artifact Mode <Understanding Artifact Builds>`
and follow the instructions at the end of the bootstrap for creating a mozconfig file.

More information :ref:`for Linux <Building Firefox On Linux>` and :ref:`for MacOS <Building Firefox On MacOS>`

Windows dependencies
--------------------

#. You need 64-bit version of Windows 7 or later.
#. Download and install `Visual Studio Community Edition. <https://visualstudio.microsoft.com/downloads/>`__
#. Finally download the `MozillaBuild Package. <https://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe>`__ Installation directory should be:

    .. code-block:: shell

        $ c:\mozilla-build\

#. Before moving on to the next steps, make sure to fulfill the :ref:`Windows prerequisites <Building Firefox On Windows>`

.. note::

    All the commands of this tutorial must be run in the shell provided with the MozillaBuild Package (start-shell.bat)

:ref:`More information <Building Firefox On Windows>`

To build & run
--------------

Once all the dependencies have been installed, run:

.. code-block:: shell

     $ ./mach build

which will check for dependencies and start the build.
This will take a while; a few minutes to a few hours depending on your hardware.

.. note::

    The default build is a compiled build with optimizations. Check out the
    :ref:`mozconfig file documentation <Configuring Build Options>`
    to see other build options. If you don't plan to change C++ or Rust code,
    an :ref:`artifact build <Understanding Artifact Builds>` will be faster.

To run it:

.. code-block:: shell

     $ ./mach run

:ref:`More information about Linux <Building Firefox On Linux>` / :ref:`More information about MacOS <Building Firefox On MacOS>`

.. _write_a_patch:

To write a patch
----------------

Make the changes you need in the codebase. You can look up UI text in `Searchfox <https://searchfox.org>`__ to find the right file.

Then:

.. code-block:: shell

    # Mercurial
    $ hg commit

    # Git
    $ git commit

.. _Commit message:

The commit message should look like:

.. code-block:: text

    Bug xxxx - Short description of your change. r?reviewer

    Optionally, a longer description of the change.

**Make sure you include the bug number and at least one reviewer (or reviewer group) in this format.**

To :ref:`find a reviewer or a review group <Getting reviews>`, the easiest way is to run
``hg log <modified-file>`` (or ``git log <modified-file>``, if
you're using git) on the relevant files, and look who usually is
reviewing the actual changes (ie not reformat, renaming of variables, etc).


To visualize your patch in the repository, run:

.. code-block:: shell

    # Mercurial
    $ hg wip

    # Git
    $ git show

:ref:`More information on how to work with stack of patches <Working with stack of patches Quick Reference>`

:ref:`More information <Mercurial Overview>`

To make sure the change follows the coding style
------------------------------------------------

To detect coding style violations, use mach lint:

.. code-block:: shell

    $ ./mach lint path/to/the/file/or/directory/you/changed

    # To get the autofix, add --fix:
    $ ./mach lint path/to/the/file/or/directory/you/changed --fix

:ref:`More information <Code quality>`

To test a change locally
------------------------

To run the tests, use mach test with the path. However, it isn’t
always easy to parse the results.

.. code-block:: shell

    $ ./mach test dom/serviceworkers

`More information <https://developer.mozilla.org/docs/Mozilla/QA/Automated_testing>`__

To test a change remotely
-------------------------

Running all the tests for Firefox takes a very long time and requires multiple
operating systems with various configurations. To build Firefox and run its
tests on continuous integration servers (CI), two commands are available:

.. code-block:: shell

    $ ./mach try chooser

To select jobs running a fuzzy search:

.. code-block:: shell

    $ ./mach try fuzzy

From `Treeherder <https://treeherder.mozilla.org/>`__ (our continuous integration system), it is also possible to attach new jobs. As every review has
a try CI run associated, it makes this work easier. See :ref:`attach-job-review` for
more information.

.. note::

    This requires `level 1 commit access <https://www.mozilla.org/about/governance/policies/commit/access-policy/>`__.

    You can ask your reviewer to submit the patch for you if you don't have that
    level of access.

:ref:`More information <Try Server>`


To submit a patch
-----------------

To submit a patch for review, we use a tool called `moz-phab <https://pypi.org/project/MozPhab/>`__.
To install it, run:

.. code-block:: shell

     $ ./mach install-moz-phab

Once you want to submit your patches (make sure you :ref:`use the right commit message <Commit message>`), run:

.. code-block:: shell

     $ moz-phab

It will publish all the currently applied patches to Phabricator and inform the reviewer.

If you wrote several patches on top of each other:

.. code-block:: shell

    $ moz-phab submit <first_revision>::<last_revision>

`More
information <https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html>`__

To update a submitted patch
---------------------------

It is rare that a reviewer will accept the first version of patch. Moreover,
as the code review bot might suggest some improvements, changes to your patch
may be required.

Run:

.. code-block:: shell

   # Mercurial
   $ hg commit --amend

   # Git
   $ git commit --amend

After amending the patch, you will need to submit it using moz-phab again.

.. warning::

    Don't use ``hg commit --amend -m`` or ``git commit --amend -m``.

    Phabricator tracks revision by editing the commit message when a
    revision is created to add a special ``Differential Revision:
    <url>`` line.

    When ``--amend -m`` is used, that line will be lost, leading to
    the creation of a new revision when re-submitted, which isn't
    the desired outcome.

If you wrote many changes, you can squash or edit commits with the
command:

.. code-block:: shell

   # Mercurial
   $ hg histedit

   # Git
   $ git rebase -i

The submission step is the same as for the initial patch.

:ref:`More information on how to work with stack of patches <Working with stack of patches Quick Reference>`

Retrieve new changes from the repository
----------------------------------------

To pull changes from the repository, run:

.. code-block:: shell

   # Mercurial
   $ hg pull --rebase

   # Git
   $ git pull --rebase

.. _push_a_change:

To push a change in the code base
---------------------------------

Once the change has been accepted and you've fixed any remaining issues
the reviewer identified, the reviewer should land the patch.

If the patch has not landed on "autoland" (the integration branch) after a few days,
feel free to contact the reviewer and/or
@Aryx or @Sylvestre on the `#introduction <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`__
channel.

The landing procedure will automatically close the review and the bug.

:ref:`More information <How to submit a patch>`

Contributing to GeckoView
-------------------------

Note that the GeckoView setup and contribution processes are different from those of Firefox;
GeckoView setup and contribution docs live in `geckoview.dev <https://geckoview.dev>`__.

More documentation about contribution
-------------------------------------

https://developer.mozilla.org/docs/Mozilla/Developer_guide/Introduction

https://mozilla-version-control-tools.readthedocs.io/en/latest/devguide/contributing.html

https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html
