Emacs
=====

ESLint
------

See `the devtools documentation <https://wiki.mozilla.org/DevTools/CodingStandards#Running_ESLint_in_Emacs>`__
that describes how to integrate ESLint into Emacs.

C/C++ Development Packages
--------------------------

General Guidelines to Emacs C++ Programming
-------------------------------------------

The following guides give an overview of the C++ editing capabilities of emacs.

It is worth reading through these guides to see what features are available.
The rest of this section is dedicated to Mozilla/Gecko specific setup for
packages.


  * `C/C++ Development Environment for Emacs <https://tuhdo.github.io/c-ide.html>`__
  * `Emacs as C++ IDE <https://syamajala.github.io/c-ide.html>`__

rtags (LLVM/Clang-based Code Indexing)
--------------------------------------

Instructions for the installation of rtags are available at the
`rtags github repo <https://github.com/Andersbakken/rtags>`__.

rtags requires a :ref:`compilation database <CompileDB back-end-compileflags>`.

In order for rtags to index correctly, included files need to be copied and
unified compilation files need to be created. Either run a full build of the
tree, or if you only want indexes to be generated for the moment, run the
following commands (assuming you're in the gecko repo root):

.. code::

    cd gecko_build_directory
    make export
    ./config.status

To increase indexing speed, it's best to remove unified build files and test
files from database updates. This can be done by creating a :code:`~/.rdmrc`
file with the following contents, with :code:`[src_dir]` replaced with either
the repo or build directory for your checkout:

.. code::

    -X */[src_dir]/*Unified*;*/[src_dir]/*/test/*;*/[src_dir]/*/tests/*

Once the rdm daemon is running, the compilation database can be added to rtags
like so:

.. code::

    rc -J [gecko_build_directory]/compile_commands.json

Note that this process will take a while initially. However, once the database
is built, it will only require incremental updates. As long as the rdm daemon
is running in the background, the database will be updated based on changes to
files.

irony (LLVM/Clang-based Code Completion)
----------------------------------------

Instructions on the installation of irony-mode are available at the
`irony-mode github repo <https://github.com/Sarcasm/irony-mode>`__.

irony-mode requires a :ref:`compilation database <CompileDB back-end-compileflags>`.

Note that irony-mode, by default, uses elisp to parse the
:code:`compile_commands.json` file. As gecko is a very large codebase, this
file can easily be multiple megabytes, which can make irony-mode take multiple
seconds to load on a gecko file.

It is recommended to use `this fork of irony-mode <https://github.com/Hylen/irony-mode/tree/compilation-database-guessing-4-pull-request>`__,
which requires the boost System and Filesystem libraries.

`Checking the bug to get this patch into the mainline of irony-mode <https://github.com/Sarcasm/irony-mode/issues/176>`__
is recommended, to see if the fork can be used or if the mainline repo can be
used. Using the Boost version of the irony-mode server brings file load times
to under 1s.

Projectile (Project Management)
-------------------------------

Instructions on the installation of projectile are available at the
`projectile github repo <https://github.com/bbatsov/projectile>`__.

Projectile comes preconfigured for many project types. Since, gecko uses its
own special build system (mach), a new project type needs to be added. This can
be done via adding the following elisp configuration command to your emacs
configuration file.

.. code::

    (projectile-register-project-type 'gecko
                                      '("mach" "moz.build")
                                      "python mach --log-no-times build"
                                      "python mach mochitest"
                                      "python mach run")

Assuming projectile-global-mode is on, this will allow projectile to run the
correct commands whenever it is working in a gecko repo.

gdb
^^^

Emacs comes with great integration with gdb, especially when using
`gdb-many-windows <https://www.gnu.org/software/emacs/manual/html_node/emacs/GDB-User-Interface-Layout.html>`__.

However, when gdb is invoked via mach, some special arguments
need to be passed in order to make sure the correct display mode is used. To
use M-x gdb with mach on firefox, use the following command:

.. code::

    gecko_repo_directory/mach run --debug --debugparams=-i=mi
