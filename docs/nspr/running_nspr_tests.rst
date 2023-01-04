Running NSPR tests
==================

NSPR has a test suite in the ``mozilla/nsprpub/pr/tests`` directory.

By default, we don't build the test programs. Running ``gmake`` in the
top-level directory (``mozilla/nsprpub``) only builds the NSPR
libraries. To build the test programs, you need to change directory to
``mozilla/nsprpub/pr/tests`` and run ``gmake``. Refer to :ref:`NSPR build
instructions` for details.

To run the test suite, run the shell script
``mozilla/nsprpub/pr/tests/runtests.sh`` in the directory where the test
program binaries reside, for example,

.. code::

    cvs -q co -r NSPR_4_6_6_RTM mozilla/nsprpub
    mkdir linux.debug
    cd linux.debug
    ../mozilla/nsprpub/configure
    gmake
    cd pr/tests
    gmake
    ../../../mozilla/nsprpub/pr/tests/runtests.sh

The output of the test suite looks like this:

.. code::

    NSPR Test Results - tests

    BEGIN                   Mon Mar 12 11:44:41 PDT 2007
    NSPR_TEST_LOGFILE       /dev/null

    Test                    Result

    accept                  Passed
    acceptread                      Passed
    acceptreademu                   Passed
    affinity                        Passed
    alarm                   Passed
    anonfm                  Passed
    atomic                  Passed
    attach                  Passed
    bigfile                 Passed
    cleanup                 Passed
    cltsrv                  Passed
    concur                  Passed
    cvar                    Passed
    cvar2                   Passed
    ...
    sprintf                 FAILED
    ...
    timetest                        Passed
    tpd                     Passed
    udpsrv                  Passed
    vercheck                        Passed
    version                 Passed
    writev                  Passed
    xnotify                 Passed
    zerolen                 Passed
    END                     Mon Mar 12 11:55:47 PDT 2007

.. _How_to_determine_if_the_test_suite_passed:

How to determine if the test suite passed
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If all the tests reported **Passed** as the results, the test suite
passed.

What if some of the tests crashed or reported **FAILED** as the results?
It doesn't necessarily mean the test suite failed because some of the
test programs are known to fail. Until the test failures are fixed, you
should run NSPR tests against **a known good version of NSPR on the same
platform**, and save the test results as the benchmark. Then you can
detect regressions of the new version by comparing its test results with
the benchmark.

.. _Known_issues:

Known issues
~~~~~~~~~~~~

Other issues with the NSPR test suite are:

#. Some of the test programs test the accuracy of the timeout of NSPR
   functions. Since none of our operating systems is a real-time OS,
   such test programs may fail when the test machine is heavily loaded.
#. Some tests, such as ``pipepong`` and ``sockpong``, should not be run
   directly. They will be invoked by their companion test programs
   (e.g., ``pipeping`` and ``sockping``). This is not an issue if you
   run ``runtests.sh`` because ``runtests.sh`` knows not to run such
   test programs directly.
