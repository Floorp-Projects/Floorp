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

For general information on using VS Code, see their `home page <https://code.visualstudio.com/>`__, `repo <https://github.com/Microsoft/vscode/>`__ and `guide to working with C++ <https://code.visualstudio.com/docs/languages/cpp>`__.

For IntelliSense to work properly, a :ref:`compilation database <CompileDB back-end / compileflags>` as described below is required. When it is present when you open the mozilla source code folder, it will be automatically detected and Visual Studio Code will ask you if it should use it, which you should confirm.

VS Code provides number of extensions for JavaScript, Rust, etc.

Useful preferences
^^^^^^^^^^^^^^^^^^

When setting the preference

.. code::

  "editor.formatOnSave": true

you might find that this isn't working on large source code files, but triggering formatting manually works. This is due to the default timeout for formatOnSave, which is quite short (750ms). You might want to increase this timeout, e.g.

.. code::

   "editor.formatOnSaveTimeout": 5000

VIM
---

There's C++ and Rust auto-completion support for VIM via
`YouCompleteMe <https://github.com/ycm-core/YouCompleteMe/>`__.

As long as that is installed and you have run :code:`./mach build` or
:code:`./mach configure`, it should work out of the box. Configuration for this lives
in :code:`.ycm_extra_conf` at the root of the repo.

Eclipse
-------

You can generate an Eclipse project by running:

.. code::

    ./mach ide eclipse

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
