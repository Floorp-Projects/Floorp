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

This linter provides a ``--fix`` option. It requires using nightly
which can be installed with:

.. code-block:: shell

   $ rustup component add clippy --toolchain nightly-x86_64-unknown-linux-gnu



Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/clippy.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/clippy/__init__.py>`_
