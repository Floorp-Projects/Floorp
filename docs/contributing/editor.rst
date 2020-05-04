Editor / IDE integration
========================

You can use any editor or IDE to contribute to Firefox, as long as it can edit
text files. However, there are some steps specific to mozilla-central that may
be useful for a better development experience. This page attempts to document
them.

.. note::

    This page is a work in progress. Please enhance this page with instructions
    for your favourite editor.

Visual Studio Code
------------------

For general information on using VS Code, see their
`home page <https://code.visualstudio.com/>`__,
`repo <https://github.com/Microsoft/vscode/>`__ and
`guide to working with C++ <https://code.visualstudio.com/docs/languages/cpp>`__.

For IntelliSense to work properly, a
:ref:`compilation database <CompileDB back-end / compileflags>` as described
below is required. When it is present when you open the mozilla source code
folder, it will be automatically detected and Visual Studio Code will ask you
if it should use it, which you should confirm.

VS Code provides number of extensions for JavaScript, Rust, etc.

Useful preferences
~~~~~~~~~~~~~~~~~~

When setting the preference

.. code::

  "editor.formatOnSave": true

you might find that this isn't working on large source code files, but triggering formatting manually works. This is due to the default timeout for formatOnSave, which is quite short (750ms). You might want to increase this timeout, e.g.

.. code::

   "editor.formatOnSaveTimeout": 5000


Recommended extensions
~~~~~~~~~~~~~~~~~~~~~~

By default, Firefox source tree comes with its own set of recommendations of Visual Studio Code extensions. They are listed in `.vscode/extensions.json <https://searchfox.org/mozilla-central/source/.vscode/extensions.json>`__.

For Rust development, the `rust-analyzer <https://marketplace.visualstudio.com/items?itemName=matklad.rust-analyzer>`__ extension is recommended.
`See the manual <https://rust-analyzer.github.io/manual.html>`__ for more information.

VIM
---

AutoCompletion
~~~~~~~~~~~~~~

There's C++ and Rust auto-completion support for VIM via
`YouCompleteMe <https://github.com/ycm-core/YouCompleteMe/>`__.

As long as that is installed and you have run :code:`./mach build` or
:code:`./mach configure`, it should work out of the box. Configuration for this lives
in :code:`.ycm_extra_conf` at the root of the repo.

ESLint
~~~~~~

The easiest way to integrate ESLint with VIM is using the `Syntastic plugin
<https://github.com/vim-syntastic/syntastic>`__.

:code:`mach eslint --setup` installs a specific ESLint version and some ESLint
plugins into the repositories' :code:`node_modules`.

You need something like this in your :code:`.vimrc` to run the checker
automatically on save:

.. code::

    autocmd FileType javascript,html,xhtml let b:syntastic_checkers = ['javascript/eslint']

You need to have :code:`eslint` in your :code:`PATH`, which you can get with
:code:`npm install -g eslint`. You need at least version 6.0.0.

You can also use something like `eslint_d
<https://github.com/mantoni/eslint_d.js#editor-integration>`__ which should
also do that automatically:

.. code::

    let g:syntastic_javascript_eslint_exec = 'eslint_d'

Emacs
-----

Mozilla-specific packages
~~~~~~~~~~~~~~~~~~~~~~~~~

dxr.el
^^^^^^

dxr.el is an elisp package that enables searching of DXR Code Indexer results
from within emacs. Using this can sometimes be easier than doing localized code
indexing with rtags, as rtags processing of code trees can be very processing
intensive.

dxr.el is available via `github repo <https://github.com/tromey/dxr.el>`__, or
via the Marmalade package manager.

ESLint
~~~~~~

See `the devtools documentation <https://wiki.mozilla.org/DevTools/CodingStandards#Running_ESLint_in_Emacs>`__
that describes how to integrate ESLint into Emacs.

C/C++ Development Packages
~~~~~~~~~~~~~~~~~~~~~~~~~~

General Guidelines to Emacs C++ Programming
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following guides give an overview of the C++ editing capabilities of emacs.

It is worth reading through these guides to see what features are available.
The rest of this section is dedicated to Mozilla/Gecko specific setup for
packages.


  * `C/C++ Development Environment for Emacs <https://tuhdo.github.io/c-ide.html>`__
  * `Emacs as C++ IDE <https://syamajala.github.io/c-ide.html>`__

rtags (LLVM/Clang-based Code Indexing)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Instructions for the installation of rtags are available at the
`rtags github repo <https://github.com/Andersbakken/rtags>`__.

rtags requires a :ref:`compilation database <CompileDB back-end / compileflags>`.

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Instructions on the installation of irony-mode are available at the
`irony-mode github repo <https://github.com/Sarcasm/irony-mode>`__.

irony-mode requires a :ref:`compilation database <CompileDB back-end / compileflags>`.

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

Eclipse
-------

You can generate an Eclipse project by running:

.. code::

    ./mach ide eclipse

See also the `Eclipse CDT <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Eclipse/Eclipse_CDT>`__ docs on MDN.

Visual Studio
-------------

You can run a Visual Studio project by running:

.. code::

    ./mach ide visualstudio

CompileDB back-end / compileflags
---------------------------------

You can generate a :code:`compile_commands.json` in your object directory by
running:

.. code::

    ./mach build-backend --backend=CompileDB

This file, the compilation database, is understood by a variety of C++ editors / IDEs
to provide auto-completion capabilities. You can also get an individual compile command by
running:

.. code::

    ./mach compileflags path/to/file

This is how the :ref:`VIM <VIM>` integration works, for example.
