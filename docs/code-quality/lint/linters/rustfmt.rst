Rustfmt
=======

`rustfmt`_ is the tool for Rust coding style.

Run Locally
-----------

The mozlint integration of rustfmt can be run using mach:

.. parsed-literal::

    $ mach lint --linter rustfmt <file paths>


Configuration
-------------

To enable rustfmt on new directory, add the path to the include
section in the `rustfmt.yml <https://searchfox.org/mozilla-central/source/tools/lint/rustfmt.yml>`_ file.


Autofix
-------

Rustfmt is reformatting the code by default. To highlight the results, we are using
the ``--check`` option.

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/rustfmt.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/rustfmt/__init__.py>`_
