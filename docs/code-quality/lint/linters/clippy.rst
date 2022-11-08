clippy
======

`clippy`_ is the tool for Rust static analysis.

Run Locally
-----------

The mozlint integration of clippy can be run using mach:

.. parsed-literal::

    $ mach lint --linter clippy <file paths>

.. note::

   clippy expects a path or a .rs file. It doesn't accept Cargo.toml
   as it would break the mozlint workflow.

Configuration
-------------

To enable clippy on new directory, add the path to the include
section in the `clippy.yml <https://searchfox.org/mozilla-central/source/tools/lint/clippy.yml>`_ file.

Autofix
-------

This linter provides a ``--fix`` option.
Please note that this option does not fix all detected issues.

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/clippy.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/clippy/__init__.py>`_
