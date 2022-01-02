Black
=====

`Black <https://black.readthedocs.io/en/stable/>`__ is a opinionated python code formatter.


Run Locally
-----------

The mozlint integration of black can be run using mach:

.. parsed-literal::

    $ mach lint --linter black <file paths>

Alternatively, omit the ``--linter black`` and run all configured linters, which will include
black.


Configuration
-------------

To enable black on new directory, add the path to the include
section in the `black.yml <https://searchfox.org/mozilla-central/source/tools/lint/black.yml>`_ file.

Autofix
-------

The black linter provides a ``--fix`` option.


Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/black.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/python/black.py>`_
