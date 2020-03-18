Flake8
======

`Flake8 <https://flake8.pycqa.org/en/latest/index.html>`__ is a popular lint wrapper for python. Under the hood, it runs three other tools and
combines their results:

* `pep8 <http://pep8.readthedocs.io/en/latest/>`__ for checking style
* `pyflakes <https://github.com/pyflakes/pyflakes>`__ for checking syntax
* `mccabe <https://github.com/pycqa/mccabe>`__ for checking complexity


Run Locally
-----------

The mozlint integration of flake8 can be run using mach:

.. parsed-literal::

    $ mach lint --linter flake8 <file paths>

Alternatively, omit the ``--linter flake8`` and run all configured linters, which will include
flake8.


Configuration
-------------

Path configuration is defined in the root `.flake8`_ file. Please update this file rather than
``tools/lint/flake8.yml`` if you need to exclude a new path. For an overview of the supported
configuration, see `flake8's documentation`_.

.. _.flake8: https://searchfox.org/mozilla-central/source/.flake8
.. _flake8's documentation: https://flake8.pycqa.org/en/latest/user/configuration.html

Autofix
-------

The flake8 linter provides a ``--fix`` option. It is based on `autopep8 <https://github.com/hhatto/autopep8>`__.
Please note that autopep8 does NOT fix all issues reported by flake8.


Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/flake8.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/python/flake8.py>`_
