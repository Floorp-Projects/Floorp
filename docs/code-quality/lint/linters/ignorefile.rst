Ignorefile Lint
===============

Ignorefile lint is a linter for ``.gitignore`` and ``.hgignore`` files,
to verify those files have equivalent entries.

Each pattern is roughly compared, ignoring punctuations, to absorb the
syntax difference.

Run Locally
-----------

The mozlint integration of ignorefile linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter ignorefile


Special syntax
--------------

The following special comment can be used to ignore the pattern in the next line.

.. parsed-literal::

    # lint-ignore-next-line: git-only
    # lint-ignore-next-line: hg-only

The next line exists only in ``.gitignore``. or ``.hgignore``.

.. parsed-literal::
    # lint-ignore-next-line: syntax-difference

The next line's pattern needs to be represented differently between
``.gitignore`` and ``.hgignore``.
This can be used when the ``.hgignore`` uses complex pattern which cannot be
represented in single pattern in ``.gitignore``.


Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/ignorefile.yml>`
* :searchfox:`Source <tools/lint/ignorefile/__init__.py>`
