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
section in the :searchfox:`black.yml <tools/lint/black.yml>` file.

Autofix
-------

The black linter provides a ``--fix`` option.


Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/black.yml>`
* :searchfox:`Source <tools/lint/python/black.py>`
