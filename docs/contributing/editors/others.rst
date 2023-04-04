Eclipse
=======

You can generate an Eclipse project by running:

.. code::

    ./mach ide eclipse

See also the `Eclipse CDT <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Eclipse/Eclipse_CDT>`__ docs on MDN.

Visual Studio
=============

You can run a Visual Studio project by running:

.. code::

    ./mach ide visualstudio

.. _CompileDB back-end-compileflags:

CompileDB back-end / compileflags
=================================

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
