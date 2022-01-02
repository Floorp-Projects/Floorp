=====================
Mozilla ESLint Plugin
=====================

This is the documentation of Mozilla ESLint PLugin.

Environments
============

The plugin implements the following environments:


.. toctree::
   :maxdepth: 2

   eslint-plugin-mozilla/environment

Rules
=====

The plugin implements the following rules:

.. toctree::
   :maxdepth: 1

   eslint-plugin-mozilla/avoid-Date-timing
   eslint-plugin-mozilla/avoid-removeChild
   eslint-plugin-mozilla/balanced-listeners
   eslint-plugin-mozilla/consistent-if-bracing
   eslint-plugin-mozilla/import-browser-window-globals
   eslint-plugin-mozilla/import-content-task-globals
   eslint-plugin-mozilla/import-globals
   eslint-plugin-mozilla/import-globals-from
   eslint-plugin-mozilla/import-headjs-globals
   eslint-plugin-mozilla/mark-test-function-used
   eslint-plugin-mozilla/no-aArgs
   eslint-plugin-mozilla/no-compare-against-boolean-literals
   eslint-plugin-mozilla/no-define-cc-etc
   eslint-plugin-mozilla/no-throw-cr-literal
   eslint-plugin-mozilla/no-useless-parameters
   eslint-plugin-mozilla/no-useless-removeEventListener
   eslint-plugin-mozilla/no-useless-run-test
   eslint-plugin-mozilla/reject-addtask-only
   eslint-plugin-mozilla/reject-chromeutils-import-params
   eslint-plugin-mozilla/reject-importGlobalProperties
   eslint-plugin-mozilla/reject-osfile
   eslint-plugin-mozilla/reject-scriptableunicodeconverter
   eslint-plugin-mozilla/reject-some-requires
   eslint-plugin-mozilla/use-cc-etc
   eslint-plugin-mozilla/use-chromeutils-import
   eslint-plugin-mozilla/use-default-preference-values
   eslint-plugin-mozilla/use-includes-instead-of-indexOf
   eslint-plugin-mozilla/use-ownerGlobal
   eslint-plugin-mozilla/use-returnValue
   eslint-plugin-mozilla/use-services
   eslint-plugin-mozilla/var-only-at-top-level

Tests
=====

The tests for eslint-plugin-mozilla are run via `mochajs`_ on top of node. Most
of the tests use the `ESLint Rule Unit Test framework`_.

.. _mochajs: https://mochajs.org/
.. _ESLint Rule Unit Test Framework: http://eslint.org/docs/developer-guide/working-with-rules#rule-unit-tests

Running Tests
-------------

The tests for eslint-plugin-mozilla are run via `mochajs`_ on top of node. Most
of the tests use the `ESLint Rule Unit Test framework`_.

The rules have some self tests, these can be run via:

.. code-block:: shell

   $ cd tools/lint/eslint/eslint-plugin-mozilla
   $ npm install
   $ npm run test

Disabling tests
---------------

In the unlikely event of needing to disable a test, currently the only way is
by commenting-out. Please file a bug if you have to do this. Bugs should be filed
in the *Testing* product under *Lint*.

.. _mochajs: https://mochajs.org/
.. _ESLint Rule Unit Test Framework: http://eslint.org/docs/developer-guide/working-with-rules#rule-unit-tests
