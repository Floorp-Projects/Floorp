Editor / IDE integration
========================

You can use any editor or IDE to contribute to Firefox, as long as it can edit
text files. However, there are some steps specific to mozilla-central that may
be useful for a better development experience. This page attempts to document
them.

.. note::

    This page is a work in progress. Please enhance this page with instructions
    for your favourite editor.

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

This file is understood by a variety of C++ editors / IDEs to provide
auto-completion capabilities. You can also get an individual compile command by
running:

.. code::

    ./mach compileflags path/to/file

This is how the :ref:`VIM <VIM>` integration works, for example.
