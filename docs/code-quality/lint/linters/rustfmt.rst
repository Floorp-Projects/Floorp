Rustfmt
=======

`rustfmt <https://github.com/rust-lang/rustfmt>`__ is the tool for Rust coding style.

Run Locally
-----------

The mozlint integration of rustfmt can be run using mach:

.. parsed-literal::

    $ mach lint --linter rustfmt <file paths>


Configuration
-------------

To enable rustfmt on new directory, add the path to the include
section in the :searchfox:`rustfmt.yml <tools/lint/rustfmt.yml>` file.


Autofix
-------

Rustfmt is reformatting the code by default. To highlight the results, we are using
the ``--check`` option.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/rustfmt.yml>`
* :searchfox:`Source <tools/lint/rust/__init__.py>`
