Running Automated JavaScript Tests
==================================

SpiderMonkey uses two separate test suites.

The `Test262 test suite <https://github.com/tc39/test262>`__ is the implementation conformance test suite for ECMA-262, the language specification for JavaScript. All JavaScript engines use Test262 to ensure that they implement JavaScript correctly. Test262 is run using ``mach jstests``.

In addition to Test262, SpiderMonkey also has a large collection of internal tests. These tests are run using ``mach jit-test``.

Both sets of tests can be run from a normal build of the JS shell.

Running Test262 locally
~~~~~~~~~~~~~~~~~~~~~~~

The jstests shell harness is in ``js/src/tests/jstests.py``. It can be invoked using

.. code:: bash

    ./mach jstests

Note that mach will generally find the JS shell itself; the --shell argument can be used to specify the location manually. All other flags will be passed along to the harness.

Test262 includes a lot of tests. When working on a specific feature, it is often useful to specify a subset of tests:

.. code:: bash

    ./mach jstests TEST_PATH_SUBSTRING [TEST_PATH_SUBSTRING_2 ...]

This runs all tests whose paths contain any TEST_PATH_SUBSTRING. To exclude tests, you can use the ``--exclude=EXCLUDED`` option. Other useful options include:

- ``-s`` (``--show-cmd``): Show the exact command line used to run each test.
- ``-o`` (``--show-output``): Print each test's output.
- ``--args SHELL_ARGS``: Extra arguments to pass to the JS shell.
- ``--rr`` Run tests under the `rr <https://rr-project.org/>`__ record-and-replay debugger.

For a complete list of options, use:

.. code:: bash

    ./mach jstests -- -h

The Test262 tests can also be run in the browser, using:

.. code:: bash

   ./mach jstestbrowser

To run a specific test, you can use the ``--filter=PATTERN`` option, where PATTERN is a RegExp pattern that is tested against ``file:///{PATH_TO_OBJ_DIR}/dist/test-stage/jsreftest/tests/jsreftest.html?test={RELATIVE_PATH_TO_TEST_FROM_js/src/tests}``.

For historical reasons, ``jstests`` also includes a SpiderMonkey specific suite of additional language feature tests in ``js/src/tests/non262``.

Running jit-tests locally
~~~~~~~~~~~~~~~~~~~~~~~~~

The jit-test shell harness is in ``js/src/jit-test/jit_test.py``. It can be invoked using

.. code:: bash

    ./mach jit-test

Basic usage of ``mach jit-test`` is similar to ``mach jstests``. A subset of tests can be specified:

.. code:: bash

    ./mach jit-test TEST_PATH_SUBSTRING [TEST_PATH_SUBSTRING_2 ...]

The ``--jitflags`` option allows you to test the JS executable with different flags. The flags are used to control which features are run, such as which JITs are enabled and how quickly they will kick in. The valid options are named bundles like 'all', 'interp', 'none', etc. See the full list in the ``-h`` / ``--help`` output.

Another helpful option specific to ``jit-tests`` is ``-R <filename>`` (or ``--retest <filename>``). The first time this is run, it runs the entire test suite and writes a list of tests that failed to the given filename. The next time it is run, it will run only the tests in the given filename, and then will write the list of tests that failed when it is done. Thus, you can keep retesting as you fix bugs, and only the tests that failed the last time will run, speeding things up a bit.

Other useful options include:

- ``-s`` (``--show-cmd``): Show the exact command line used to run each test.
- ``-o`` (``--show-output``): Print each test's output.
- ``--args SHELL_ARGS``: Extra arguments to pass to the JS shell.
- ``--debug-rr`` Run a test under the `rr <https://rr-project.org/>`__ record-and-replay debugger.
- ``--cgc`` Run a test with frequent compacting GCs (equivalent to ``SM(cgc)``)

Adding new jit-tests
~~~~~~~~~~~~~~~~~~~~

Creating new tests for jit-tests is easy. Just add a new JS file in an appropriate directory under ``js/src/jit-test/tests``. (By default, tests should go in ``test/basic``.) The test harness will automatically find the test and run it. The test is considered to pass if the exit code of the JS shell is zero (i.e., JS didn't crash and there were no JS errors). Use the ``assertEq`` function to verify values in your test.

There are some advanced options for tests. See the README (in ``js/src/jit-tests``) for details.

Running jstests on Treeherder
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Treeherder, jstests run in the browser are shown as ``R(J)`` (search for ``jsreftest`` in ``mach try fuzzy``). SpiderMonkey shell jobs are shown as ``SM(...)``; most of them include JS shell runs of both jstests and jit-tests.
