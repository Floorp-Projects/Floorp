ESLint
======

`ESLint`_ is a popular linter for JavaScript. The ESLint integration also uses
`Prettier`_ to enforce code formatting.

Run Locally
-----------

The mozlint integration of ESLint can be run using mach:

.. parsed-literal::

    $ mach lint --linter eslint <file paths>

Alternatively, omit the ``--linter eslint`` and run all configured linters, which will include
ESLint.

ESLint also supports the ``--fix`` option to autofix most errors raised from most of the rules.

See the `Usage guide`_ for more options.

Understanding Rules and Errors
------------------------------

* Only some files are linted, see the `configuration`_ for details.

  * By design we do not lint/format reftests not crashtests as these are specially crafted tests.

* If you don't understand a rule, you can look it in `eslint.org's rule list`_ for more
  information about it.
* For Mozilla specific rules (with the mozilla/ prefix), see these for more details:

  * `eslint-plugin-mozilla`_
  * `eslint-plugin-spidermonkey-js`_

Common Issues and How To Solve Them
-----------------------------------

My editor says that ``mozilla/whatever`` is unknown
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Run ``./mach eslint --setup``, and restart your editor.

My editor doesn't understand a new global I've just added (e.g. to a content file or head.js file)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* This is a limitation which is a mixture of our ESLint setup and how we share globals across files.
* Restarting your editor should pick up the new globals.
* You can always double check via `./mach lint --linter eslint <file path>` on the command line.

This code shouldn't be linted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* If it is a third-party piece of code, please add it to `ThirdPartyPaths.txt`_.
* If it is pre-generated file or intentionally invalid, please add it to `.eslintignore`_

I'm using an es6 module
^^^^^^^^^^^^^^^^^^^^^^^

* ESlint will need to be `informed that it is a module`_. You have to do this from
  the configuration itself.

I have valid code that is failing the ``no-undef`` rule or can't be parsed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Please do not add this to `.eslintignore`_. Generally this can be fixed, if the following
  tips don't help, please `seek help`_.
* If you are adding a new test directory, make sure its name matches one of the
  `patterns in .eslintrc.js`_. If you really can't match those, then you may need
  to add a separate `test specific .eslintrc.js file (example)`_.
* If you are writing a frame script you may need to tell ESLint to use the `frame script environment`_:

  * ``/* eslint-env mozilla/frame-script */``

* If you are writing a worker, then you may need to use the worker or chrome-worker environment:

  * ``/* eslint-env worker */``
  * ``/* eslint-env mozilla/chrome-worker */``

* I use ``Services.scriptloader.loadSubScript``:

  * ``/* import-globals-from relative/path/to/file.js``

Configuration
-------------

The global configuration file lives in ``topsrcdir/.eslintrc``. This global configuration can be
overridden by including an ``.eslintrc`` in the appropriate subdirectory. For an overview of the
supported configuration, see `ESLint's documentation`_.

Please keep differences in rules across the tree to a minimum. We want to be consistent to
make it easier for developers.

Sources
-------

* `Configuration (YAML)`_
* `Source`_

.. toctree::
   :hidden:

   eslint-plugin-mozilla
   eslint-plugin-spidermonkey-js

.. _ESLint: http://eslint.org/
.. _Prettier: https://prettier.io/
.. _Usage guide: ../usage.html
.. _ESLint's documentation: http://eslint.org/docs/user-guide/configuring
.. _configuration: https://searchfox.org/mozilla-central/source/tools/lint/eslint.yml
.. _eslint.org's rule list: http://eslint.org/docs/rules/
.. _eslint-plugin-mozilla: eslint-plugin-mozilla.html
.. _eslint-plugin-spidermonkey-js: eslint-plugin-spidermonkey-js.html
.. _ThirdPartyPaths.txt: https://searchfox.org/mozilla-central/source/tools/rewriting/ThirdPartyPaths.txt
.. _informed that it is a module: https://searchfox.org/mozilla-central/rev/9399e5832979755cd340383f4ca4069dd5fc7774/browser/base/content/.eslintrc.js
.. _.eslintignore: https://searchfox.org/mozilla-central/source/.eslintignore
.. _seek help: ../index.html#getting-help
.. _patterns in .eslintrc.js: https://searchfox.org/mozilla-central/rev/9399e5832979755cd340383f4ca4069dd5fc7774/.eslintrc.js#24-38
.. _test specific .eslintrc.js file (example): https://searchfox.org/mozilla-central/source/browser/base/content/test/about/.eslintrc.js
.. _frame script environment: http://localhost:5500/code-quality/lint/linters/eslint-plugin-mozilla/environment.html#frame-script
.. _Configuration (YAML): https://searchfox.org/mozilla-central/source/tools/lint/eslint.yml
.. _Source: https://searchfox.org/mozilla-central/source/tools/lint/eslint/__init__.py
