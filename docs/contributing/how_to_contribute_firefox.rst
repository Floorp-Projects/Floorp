How to contribute to Firefox
============================


The whole process is a bit long, and it will take time to get things right.
If at any point you are stuck, please don't hesitate to ask at `https://chat.mozilla.org <https://chat.mozilla.org>`__
in the `#introduction <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`__ channel.

Clone the sources
-----------------

You can use either mercurial or git. Mercurial is the canonical version control
system.

.. code-block:: shell

    $ hg clone https://hg.mozilla.org/mozilla-central/

For git, see the `git cinnabar documentation <https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-git-workflow-for-Gecko-development>`__

The clone should be around 30 minutes (depending on your connection) and
the repository should be less than 5GB (~ 20GB after the build).

If you have any network connection issues and cannot clone with command, try :ref:`Mercurial bundles <Mercurial bundles>`.

:ref:`More information <Mercurial Overview>`

Install dependencies
--------------------

Firefox provides a mechanism to install all dependencies; in the source tree:

.. code-block:: shell

     $ ./mach bootstrap

The default options are recommended.
If you're not planning to write C++ or Rust code, select :ref:`Artifact Mode <Artifact builds>`
and follow the instructions at the end of the bootstrap for creating a mozconfig file.

:ref:`More information <Linux build preparation>`

Windows dependencies
--------------------

#. You need 64-bit version of Windows 7 or later.
#. Download and install `Visual Studio. <https://visualstudio.microsoft.com/downloads/>`__
#. Finally download the `MozillaBuild Package. <https://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe>`__ Installation directory should be:

.. code-block:: shell

    $ c:\mozilla-build\

`More
information <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Windows_Prerequisites>`__


To build & run
--------------

Once all the dependencies have been installed, run:

.. code-block:: shell

     $ ./mach build

which will check for dependencies and start the build.
This will take a while; a few minutes to a few hours depending on your hardware.

.. note::

    The default build is a compiled build with optimizations. Check out the
    `mozconfig file documentation <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options>`__
    to see other build options. If you don't plan to change C++ or Rust code,
    an :ref:`artifact build <Artifact builds>` will be faster.

To run it:

.. code-block:: shell

     $ ./mach run

`More
information <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build/Linux_and_MacOS_build_preparation>`__


To write a patch
----------------

Make the changes you need in the codebase. You can look up UI text in `Searchfox <https://searchfox.org>`__ to find the right file.

Then:

.. code-block:: shell

    # Mercurial
    $ hg commit

    # Git
    $ git commit

The commit message should look like:

.. code-block::

    Bug xxxx - Short description of your change. r?reviewer

    Optionally, a longer description of the change.

To :ref:`find a reviewer or a review group <Getting reviews>`, the easiest way is to do
``hg log <modified-file>`` (or ``git log <modified-file>``, if
you're using git) on the relevant files, and look who usually is
reviewing the actual changes (ie not reformat, renaming of variables, etc).

To visualize your patch in the repository, run:

.. code-block:: shell

    # Mercurial
    $ hg wip

    # Git
    $ git show


:ref:`More information <Mercurial Overview>`


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

From Treeherder, it is also possible to attach new jobs. As every review has
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

If you wrote many changes, you can squash or edit commits with the
command:

.. code-block:: shell

   # Mercurial
   $ hg histedit

   # Git
   $ git rebase -i

The submission step is the same as for the initial patch.

Retrieve new changes from the repository
----------------------------------------

To pull changes from the repository, run:

.. code-block:: shell

   # Mercurial
   $ hg pull --rebase

   # Git
   $ git pull --rebase

To push a change in the code base
---------------------------------

Once the change has been accepted, ask the reviewer if they could land
the change. They don’t have an easy way to know if a contributor has
permission to land it or not.

If the reviewer does not land the patch after a few days, add
the *Check-in Needed* Tags to the review (*Edit Revision*).

The landing procedure will automatically close the review and the bug.

`More
information <https://developer.mozilla.org/docs/Mozilla/Developer_guide/How_to_Submit_a_Patch#Submitting_the_patch>`__

Contributing to GeckoView
-------------------------

GeckoView information and contribution docs live in `geckoview.dev <https://geckoview.dev>`__.

More documentation about contribution
-------------------------------------

https://developer.mozilla.org/docs/Mozilla/Developer_guide/Introduction

https://mozilla-version-control-tools.readthedocs.io/en/latest/devguide/contributing.html

https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html
