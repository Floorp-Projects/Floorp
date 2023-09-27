ESLint
======

`ESLint`__ is a popular linter for JavaScript. The ESLint integration also uses
`Prettier`_ to enforce code formatting.

.. contents::
    :local:

Run Locally
-----------

The mozlint integration of ESLint can be run using mach:

.. parsed-literal::

    $ mach lint --linter eslint <file paths>

Alternatively, omit the ``--linter eslint`` and run all configured linters, which will include
ESLint.

ESLint also supports the ``--fix`` option to autofix most errors raised from most of the rules.

See the `Usage guide`_ for more options.

Custom Configurations
---------------------

Our ESLint configuration has a number of custom configurations that define
globals and rules for various code based on the pattern file path and names.

Using the correct patterns helps ESLint to know about the correct details, so
that you don't get warnings about undefined or unused variables.

* ``.mjs`` - A module file.
* ``.sys.mjs`` - A system module, this is typically a singleton in the process it is loaded into.
* ``.worker.(m)js`` - A file that is a web worker.

  * Workers that use ctypes should use ``/* eslint-env mozilla/chrome-worker */``

* Test files, see the section on :ref:`adding tests <adding-tests>`


Understanding Rules and Errors
------------------------------

* Only some files are linted, see the :searchfox:`configuration <tools/lint/eslint.yml>` for details.

  * By design we do not lint/format reftests not crashtests as these are specially crafted tests.

* If you don't understand a rule, you can look it in `eslint.org's rule list`_ for more
  information about it.
* For Mozilla specific rules (with the mozilla/ prefix), see these for more details:

  * `eslint-plugin-mozilla`_
  * `eslint-plugin-spidermonkey-js`_

.. _eslint_common_issues:

Enabling new rules and adding plugins
-------------------------------------

Please see `this page for enabling new rules <eslint/enabling-rules.html>`_.

Common Issues and How To Solve Them
-----------------------------------

My editor says that ``mozilla/whatever`` is unknown
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Run ``./mach eslint --setup``, and restart your editor.

My editor doesn't understand a new global I've just added (e.g. to a content file or head.js file)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* This is a limitation which is a mixture of our ESLint setup and how we share globals across files.
* Restarting your editor should pick up the new globals.
* You can always double check via ``./mach lint --linter eslint <file path>`` on the command line.

I am getting a linter error "Unknown Services member property"
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Make sure to add any new Services to ``tools/lint/eslint/eslint-plugin-mozilla/lib/services.json``. For example by copying from
``<objdir>/xpcom/components/services.json`` after a build.

.. _adding-tests:

I'm adding tests, how do I set up the right configuration?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Please note we prefer the tests to be in named directories as this makes it
easier to identify the types of tests developers are working with. Additionally,
it is not possible to scope ESLint rules to individual files based on .ini
files without a build step that would break editors, or an expensive loading
cycle.

* If the directory path of the tests is one of the `known ones`_, then ESLint will
  do the right thing for that test type. This is the preferred option.

  * For example placing xpcshell-tests in ``browser/components/foo/test/unit/``
    will set up ESLint correctly.

* If you really can't match the directory name, e.g. like the
  ``browser/base/content/tests/*``, then you'll need to add a new entry in
  :searchfox:`.eslintrc-test-paths.js <.eslintrc-test-paths.js>`.

Please do not add new cases of multiple types of tests within a single directory,
this is `difficult for ESLint to handle`_. Currently this may cause:

* Rules to be incorrectly applied to the wrong types of test file.
* Extra definitions for globals in tests which means that the no undefined variables
  rule does not get triggered in some cases.

This code should neither be linted nor formatted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* If it is a third-party piece of code, please add it to :searchfox:`ThirdPartyPaths.txt <tools/rewriting/ThirdPartyPaths.txt>`.
* If it is a generated file, please add it to :searchfox:`Generated.txt <tools/rewriting/Generated.txt>`.
* If intentionally invalid, please add it to :searchfox:`.eslintignore <.eslintignore>`.

This code shouldn't be formatted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The vast majority of code should be formatted, however we allow some limited
cases where it makes sense, for example:

* A table in an array where laying it out in a table fashion makes it more readable.
* Other structures or function calls where layout is more readable in a particular format.

To disable prettier for code like this, ``// prettier-ignore`` may be used on
the line previous to where you want it disabled.
See the `prettier ignore docs`_ for more information.

I have valid code that is failing the ``no-undef`` rule or can't be parsed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Please do not add this to :searchfox:`.eslintignore <.eslintignore>`. Generally
  this can be fixed, if the following tips don't help, please `seek help`_.
* If you are adding a new test directory, see the :ref:`section above <adding-tests>`

* If you are writing a script loaded into special environment (e.g. frame script) you may need to tell ESLint to use the `environment definitions`_ for each case:

  * ``/* eslint-env mozilla/frame-script */``

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

* :searchfox:`Configuration (YAML) <tools/lint/eslint.yml>`
* :searchfox:`Source <tools/lint/eslint/__init__.py>`

Builders
--------

`Mark Banner (standard8) <https://people.mozilla.org/s?query=standard8>`__ owns
the builders. Questions can also be asked on #lint:mozilla.org on Matrix.

ESLint (ES)
^^^^^^^^^^^

This is a tier-1 task. For test failures the patch causing the
issue should be backed out or the issue fixed.

Some failures can be fixed with ``./mach eslint --fix path/to/file``.

For test harness issues, file bugs in Developer Infrastructure :: Lint and Formatting.

ESLint-build (ES-B)
^^^^^^^^^^^^^^^^^^^

This is a tier-2 task that is run once a day at midnight UTC via a cron job.

It currently runs the ESLint rules plus two additional rules:

* `valid-ci-uses <eslint-plugin-mozilla/valid-ci-uses.html>`__
* `valid-services-property <eslint-plugin-mozilla/valid-services-property.html>`__

These are two rules that both require build artifacts.

To run them manually, you can run:

``MOZ_OBJDIR=objdir-ff-opt ./mach eslint --rule "mozilla/valid-ci-uses: error" --rule "mozilla/valid-services-property: error" *``

For test failures, the regression causing bug may be able to be found by:

    * Determining if the file where the error is reported has been changed recently.
    * Seeing if an associated ``.idl`` file has been changed.

If no regressing bug can easily be found, file a bug in the relevant
product/component for the file where the failure is and cc :standard8.

For test harness issues, file bugs in Developer Infrastructure :: Lint and Formatting.

.. toctree::
   :hidden:

   eslint-plugin-mozilla
   eslint-plugin-spidermonkey-js

.. __: https://eslint.org/
.. _Prettier: https://prettier.io/
.. _Usage guide: ../usage.html
.. _ESLint's documentation: https://eslint.org/docs/user-guide/configuring
.. _eslint.org's rule list: https://eslint.org/docs/rules/
.. _eslint-plugin-mozilla: eslint-plugin-mozilla.html
.. _eslint-plugin-spidermonkey-js: eslint-plugin-spidermonkey-js.html
.. _informed that it is a module: https://searchfox.org/mozilla-central/rev/9399e5832979755cd340383f4ca4069dd5fc7774/browser/base/content/.eslintrc.js
.. _seek help: ../index.html#getting-help
.. _patterns in .eslintrc.js: https://searchfox.org/mozilla-central/rev/9399e5832979755cd340383f4ca4069dd5fc7774/.eslintrc.js#24-38
.. _environment definitions: ./eslint-plugin-mozilla/environment.html
.. _known ones: https://searchfox.org/mozilla-central/rev/287583a4a605eee8cd2d41381ffaea7a93d7b987/.eslintrc.js#24-40
.. _difficult for ESLint to handle: https://bugzilla.mozilla.org/show_bug.cgi?id=1379669
.. _prettier ignore docs: https://prettier.io/docs/en/ignore.html
