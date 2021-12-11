pylint
======

`pylint <https://www.pylint.org/>`__ is a popular linter for python. It is now the default python
linter in VS Code.

Please note that we also have :ref:`Flake8` available as a linter.

Run Locally
-----------

The mozlint integration of pylint can be run using mach:

.. parsed-literal::

    $ mach lint --linter pylint <file paths>



Configuration
-------------

To enable pylint on new directory, add the path to the include
section in the `pylint.yml <https://searchfox.org/mozilla-central/source/tools/lint/pylint.yml>`_ file.

We enabled the same Pylint rules as `VS Code <https://code.visualstudio.com/docs/python/linting#_pylint>`_.
See in `pylint.py <https://searchfox.org/mozilla-central/source/tools/lint/python/pylint.py>`_ for the full list

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/pylint.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/python/pylint.py>`_
