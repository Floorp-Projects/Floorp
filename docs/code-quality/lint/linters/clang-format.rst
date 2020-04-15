clang-format
============

`clang-format <https://clang.llvm.org/docs/ClangFormat.html>`__ is a tool to reformat C/C++ to the right coding style.

Run Locally
-----------

The mozlint integration of clang-format can be run using mach:

.. parsed-literal::

    $ mach lint --linter clang-format <file paths>


Configuration
-------------

To enable clang-format on new directory, add the path to the include
section in the `clang-format.yml <https://searchfox.org/mozilla-central/source/tools/lint/clang-format.yml>`_ file.

While excludes: will work, this linter will read the ignore list from `.clang-format-ignore file <https://searchfox.org/mozilla-central/source/.clang-format-ignore>`_
at the root directory. This because it is also used by the ./mach clang-format -p command.

Autofix
-------

clang-format can reformat the code with the option `--fix` (based on the upstream option `-i`).
To highlight the results, we are using the ``--dry-run`` option (from clang-format 10).

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/clang-format.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/clang-format/__init__.py>`_
