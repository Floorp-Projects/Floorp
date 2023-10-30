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
   :glob:

   eslint-plugin-mozilla/rules/*

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
