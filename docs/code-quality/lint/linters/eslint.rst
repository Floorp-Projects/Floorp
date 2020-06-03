ESLint
======

`ESLint <http://eslint.org/>`__ is a popular linter for JavaScript.

Run Locally
-----------

The mozlint integration of `ESLint <http://eslint.org/>`__ can be run using mach:

.. parsed-literal::

    $ mach lint --linter eslint <file paths>

Alternatively, omit the ``--linter eslint`` and run all configured linters, which will include
ESLint.


Configuration
-------------

The ESLint mozilla-central integration uses a skip list to exclude certain directories from being
linted. This lives in ``topsrcdir/.eslintignore``. If you don't wish your directory to be linted, it
must be added here.

The global configuration file lives in ``topsrcdir/.eslintrc``. This global configuration can be
overridden by including an ``.eslintrc`` in the appropriate subdirectory. For an overview of the
supported configuration, see `ESLint's documentation <http://eslint.org/docs/user-guide/configuring>`__.


Autofix
-------

The eslint linter provides a ``--fix`` option. It is based on the upstream option.

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/eslint.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/eslint/__init__.py>`_


ESLint Plugin Mozilla
---------------------

In addition to default ESLint rules, there are several Mozilla-specific rules that are defined in
the :doc:`Mozilla ESLint Plugin <eslint-plugin-mozilla>`.

ESLint Plugin SpiderMonkey JS
-----------------------------

In addition to default ESLint rules, there is an extra processor for SpiderMonkey
code :doc:`Mozilla ESLint SpiderMonkey JS <eslint-plugin-spidermonkey-js>`.


.. toctree::
   :hidden:

   eslint-plugin-mozilla
   eslint-plugin-spidermonkey-js
