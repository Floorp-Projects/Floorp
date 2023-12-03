=====================================
Formatting C++ Code With clang-format
=====================================

Mozilla uses the Google coding style for whitespace, which is enforced
using `clang-format <https://clang.llvm.org/docs/ClangFormat.html>`__. A
specific version of the binary will be installed when
``./mach clang-format`` or ``./mach bootstrap`` are run. We build our
own binaries and update them as needed.

Options are explicitly defined `in clang-format
itself <https://github.com/llvm-mirror/clang/blob/e8a55f98df6bda77ee2eaa7f7247bd655f79ae0e/lib/Format/Format.cpp#L856>`__.
If the options are changed in clang upstream, this might cause some
changes in the Firefox tree. For this reason, it is best to use the
mozilla-provided binaries.

Manual formatting
-----------------

We provide a mach subcommand for running clang-format from the
command-line. This wrapper handles ensuring the correct version of
clang-format is installed and run.

If clang-format isn’t installed, the binaries will be automatically
downloaded from taskcluster and installed into ~/.mozbuild. We build our
own clang-format binaries.


Formatting local changes
~~~~~~~~~~~~~~~~~~~~~~~~

::

   $ ./mach clang-format

When run without arguments, it will run on a local diff. This could miss
some reformatting (for example, when blocks are touched).
(`searchfox <https://searchfox.org/mozilla-central/rev/501eb4718d73870892d28f31a99b46f4783efaa0/python/mozbuild/mozbuild/code-analysis/mach_commands.py#1620>`__)


Formatting specific paths
~~~~~~~~~~~~~~~~~~~~~~~~~

::

   $ ./mach clang-format -p <path>     # Format <path> in-place
   $ ./mach clang-format -p <path> -s  # Show changes

The command also accepts a ``-p`` argument to reformat a specific
directory or file, and a ``-s`` flag to show the changes instead of
applying them to the working directory
(`searchfox <https://searchfox.org/mozilla-central/rev/501eb4718d73870892d28f31a99b46f4783efaa0/python/mozbuild/mozbuild/code-analysis/mach_commands.py#1633>`__)


Formatting specific commits / revisions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

   $ ./mach clang-format -c HEAD # Format a single git commit
   $ ./mach clang-format -c HEAD~~..HEAD # Format a range of git commits
   $ ./mach clang-format -c . # Format a single mercurial revision

The command accepts a ``-c`` argument that takes a revision number or
commit range, and will format the lines modified by those commits.
(`searchfox <https://searchfox.org/mozilla-central/rev/501eb4718d73870892d28f31a99b46f4783efaa0/python/mozbuild/mozbuild/code-analysis/mach_commands.py#1635>`__)


Scripting Clang-Format
~~~~~~~~~~~~~~~~~~~~~~

Clang format expects that the path being passed to it is the path
on-disk. If this is not the case, for example when formatting a
temporary file, the "real" path must be specified. This can be done with
the ``--assume-filename <path>`` argument.


Configuring the clang-format commit hook
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To run clang-format at commit phase, run ``mach bootstrap`` or just add
the following line in the ``hgrc`` file:

.. code:: ini

   [extensions]
   clang-format = ~/.mozbuild/version-control-tools/hgext/clang-format

We use a hg extension as they are more flexible than hooks.

With git, the configuration is the following:

::

   # From the root git directory:
   $ ln -s $(pwd)/tools/lint/hooks_clang_format.py .git/hooks/pre-commit

You'll likely need to install the ``python-hglib`` package for your OS,
or else you may get errors like ``abort: No module named hglib.client!``
when you try to commit.


Editor integration
------------------

It is possible to configure many editors to support running
``clang-format`` automatically on save, or when run from within the
editor.


Editor plugins
~~~~~~~~~~~~~~

-  `Atom <https://atom.io/packages/clang-format>`__
-  `BBEdit <http://clang.llvm.org/docs/ClangFormat.html#bbedit-integration>`__

   -  `clang-format-bbedit.applescript <https://raw.githubusercontent.com/llvm-mirror/clang/master/tools/clang-format/clang-format-bbedit.applescript>`__

-  Eclipse

   -  Install the
      `CppStyle <https://marketplace.eclipse.org/content/cppstyle>`__
      plugin
   -  In Preferences -> C/C++ -> CppStyle, set the clang-format path to
      ~/.mozbuild/clang-tools/clang-tidy/bin/clang-format
   -  (Optional) check "Run clang-format on file save"

-  `Emacs <http://clang.llvm.org/docs/ClangFormat.html#emacs-integration>`__

   -  `clang-format.el <https://raw.githubusercontent.com/llvm-mirror/clang/master/tools/clang-format/clang-format.el>`__
      (Or install
      `clang-format <http://melpa.org/#/clang-format>`__ from MELPA)
   -  `google-c-style <http://melpa.org/#/google-c-style>`__ from MELPA

-  `Sublime Text <https://packagecontrol.io/packages/Clang%20Format>`__

   -  `alternative
      tool <https://github.com/rosshemsley/SublimeClangFormat>`__

-  `Vim <http://clang.llvm.org/docs/ClangFormat.html#vim-integration>`__

   -  `clang-format.py <https://raw.githubusercontent.com/llvm-mirror/clang/master/tools/clang-format/clang-format.py>`__
   -  `vim-clang-format <https://github.com/rhysd/vim-clang-format>`__

-  `Visual
   Studio <https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.ClangFormat>`__

   -  `llvm.org plugin <http://llvm.org/builds/>`__
   -  `Integrated support in Visual Studio
      2017 <https://blogs.msdn.microsoft.com/vcblog/2018/03/13/clangformat-support-in-visual-studio-2017-15-7-preview-1/>`__

-  `Visual Studio
   Code <https://marketplace.visualstudio.com/items?itemName=xaver.clang-format>`__
-  `XCode <https://github.com/travisjeffery/ClangFormat-Xcode>`__
-  `Script for patch
   reformatting <http://clang.llvm.org/docs/ClangFormat.html#script-for-patch-reformatting>`__

   -  `clang-format-diff.py <https://raw.githubusercontent.com/llvm-mirror/clang/master/tools/clang-format/clang-format-diff.py>`__


Configuration
~~~~~~~~~~~~~

These tools generally run clang-format themselves, and won't use
``./mach clang-format``. The binary installed by our tooling will be
located at ``~/.mozbuild/clang-tools/clang-tidy/bin/clang-format``.

You typically shouldn't need to specify any other special configuration
in your editor besides the clang-format binary. Most of the
configuration that clang-format relies on for formatting is stored
inside our source tree. More specifically, using the .clang-format file
located in the root of the repository. Please note that this doesn't
include the list of ignored files and directories (provided by
.clang-format-ignore which is a feature provided by the mach command
wrapper).

Coding style configuration is done within clang-format itself. When we
change the configuration (incorrect configuration, new feature in clang,
etc), we use `local
overrides <https://searchfox.org/mozilla-central/rev/501eb4718d73870892d28f31a99b46f4783efaa0/.clang-format>`__.


Ignored files & directories
~~~~~~~~~~~~~~~~~~~~~~~~~~~

We maintain a `list of ignored directories and
files <https://searchfox.org/mozilla-central/rev/501eb4718d73870892d28f31a99b46f4783efaa0/.clang-format-ignore>`__,
which is used by ``./mach clang-format``. This is generally only used
for code broken by clang-format, and third-party code.


Ignored code hunks
~~~~~~~~~~~~~~~~~~

Sections of code may have formatting disabled using comments. If a
section must not be formatted, the following comments will disable the
reformat:

::

   // clang-format off
   my code which should not be reformatted
   // clang-format on

You can find an `example of code not
formatted <https://searchfox.org/mozilla-central/rev/501eb4718d73870892d28f31a99b46f4783efaa0/xpcom/io/nsEscape.cpp#22>`__.


Merging formatted and unformatted code
--------------------------------------

During the transition to using chromium style enforced by clang-format
for all code in tree, it will often be necessary to rebase non-formatted
code onto a formatted tree.


Mercurial
~~~~~~~~~

The ``format-source`` extension, now bundled with
``version-control-tools``, and installed by ``./mach bootstrap``, may be
used to seamlessly handle this situation. More details may be found in
this
`document <https://docs.google.com/document/d/13AwAsvKMhH0mflDlfatBqn6LmZHiQih76oxM4zfrPl4/edit>`__.

The parent changeset of the reformat has been tagged as
``PRE_TREEWIDE_CLANG_FORMAT``.


Git
~~~

To perform a rebase onto mozilla-central after the merge, a handy merge
driver, ``clang-format-merge``, has been written:

.. code:: shell

   $ git clone https://github.com/emilio/clang-format-merge
   $ /path/to/clang-format-merge/git-wrapper rebase <upstream>

The wrapper should clean up after itself, and the clone may be deleted
after the rebase is complete.


Ignore lists
------------

To make sure that the blame/annotate features of Mercurial or git aren't
affected. Two files are maintained to keep track of the reformatting
commits.


With Mercurial
~~~~~~~~~~~~~~

| The list is stored in
  `https://searchfox.org/mozilla-central/source/.hg-annotate-ignore-revs </en-US/docs/>`__
| Commit messages should also contain the string ``# ignore-this-changeset``

The syntax in this file is generated using the following syntax:

::

   $ hg log --template '{node} - {author|person} - {desc|strip|firstline}\n'

With git
~~~~~~~~

The list is stored in
`https://searchfox.org/mozilla-central/source/.git-blame-ignore-revs </en-US/docs/>`__
and contains git revisions for both gecko-dev and the git cinnabar
repository.
