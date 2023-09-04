GTest
=====

GTest (googletest) is Google's framework for writing C++ tests on a
variety of platforms (Linux, Mac OS X, Windows, ...).
Based on the xUnit architecture, it supports automatic test
discovery, a rich set of assertions, user-defined assertions, death
tests, fatal and non-fatal failures, value- and type-parameterized
tests, various options for running the tests, and XML test report
generation.

Integration
-----------

GTest is run as a standard test task on Win/Mac/Linux and Android, under
treeherder symbol 'GTest'.


Running tests
-------------

The Firefox build process will build GTest on supported platforms as
long as you don't disable tests in your mozconfig. However xul-gtest will
only be built when tests are required to save an expensive second
linking process.

To run the unit tests use 'mach gtest' when invoking Gecko.

Running selected tests
~~~~~~~~~~~~~~~~~~~~~~

Tests can be selected using mach. You can also use environment variables
support by GTest. See `Running Test Programs: Running a Subset of the
Tests <https://github.com/google/googletest/blob/master/docs/advanced.md#running-a-subset-of-the-tests>`__
for more details.

::

   mach gtest Moz2D.*


Configuring GTest
~~~~~~~~~~~~~~~~~

GTest can be controlled from other environment variables. See `Running
Test Programs: Advanced
Options <https://github.com/google/googletest/blob/master/docs/advanced.md#running-test-programs-advanced-options>`__
for more details.


Debugging a GTest Unit Test
---------------------------

To debug a gtest, pass --debug to the normal command.

.. code-block:: shell

   ./mach gtest --debug [ Prefix.Test ]

If that doesn't work, you can try running the firefox binary under the
debugger with the MOZ_RUN_GTEST environment variable set to 1.

.. code-block:: shell

   MOZ_RUN_GTEST=1 ./mach run --debug [--debugger gdb]

.. warning::

   Don't forget to build + run 'mach gtest' to relink when using
   MOZ_RUN_GTEST since it's not part of a top level build.

Note that this will load an alternate libxul - the one which has the
test code built in, which resides in a gtest/subdirectory of your
objdir. This gtest-enabled libxul is not built as part of the regular
build, so you must ensure that it is built before running the above
command. A simple way to do this is to just run "mach gtest" which will
rebuild libxul and run the tests. You can also extract the commands
needed to just rebuild that libxul `from
mach <https://hg.mozilla.org/mozilla-central/file/3673d2c688b4/python/mozbuild/mozbuild/mach_commands.py#l486>`__
and run those directly. Finally, note that you may have to run through
the tests once for gdb to load all the relevant libraries and for
breakpoint symbols to resolve properly.

Note that you can debug a subset of the tests (including a single test)
by using the GTEST_FILTER environment variable:

.. code-block:: shell

   GTEST_FILTER='AsyncPanZoom*' MOZ_RUN_GTEST=1 ./mach run --debug [--debugger gdb]


Debugging with Xcode
~~~~~~~~~~~~~~~~~~~~

See :ref:`Debugging On macOS` for initial
setup. You'll likely want to create a separate scheme for running GTest
("Product" > "Scheme" > "New Scheme…"). In addition to GTEST_FILTER, Set
the following environment variables:

::

   MOZ_XRE_DIR=/path-to-object-directory/obj-ff-dbg/dist/bin
   MOZ_RUN_GTEST=True

and under the "Options" tab for the scheme, set the working directory
to:

::

   ☑️ Use custom working directory: /path-to-object-directory/obj-ff-dbg/_tests/gtest


Debugging with Visual Studio Code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add a configuration like this to your launch.json file (you can edit it
via Run / Open Configurations):

::

           {
               "name": "(gdb) Launch gtest",
               "type": "cppdbg",
               "request": "launch",
               "program": "${workspaceFolder}/obj-x86_64-pc-linux-gnu/dist/bin/firefox",
               "args": [],
               "stopAtEntry": false,
               "cwd": "${workspaceFolder}/obj-x86_64-pc-linux-gnu/_tests/gtest",
               "environment": [{"name": "MOZ_RUN_GTEST", "value": "True"},
                               {"name": "GTEST_FILTER", "value": "AsyncPanZoom*"}],
               "externalConsole": false,
               "MIMode": "gdb",
               "setupCommands": [
                   {
                       "description": "Enable pretty-printing for gdb",
                       "text": "-enable-pretty-printing",
                       "ignoreFailures": true
                   }
               ]
           },


Writing a GTest Unit Test
-------------------------

Most of the `GTest
documentation <https://github.com/google/googletest/blob/master/googletest/README.md>`__
will apply here. The `GTest
primer <https://github.com/google/googletest/blob/master/docs/primer.md>`__
is a recommended read.

.. warning::

   GTest will run tests in parallel. Don't add unit tests that are not
   threadsafe, such as tests that require focus or use specific sockets.

.. warning::

   GTest will run without initializing mozilla services. Initialize and
   tear down any dependencies you have in your test fixtures. Avoid
   writing integration tests and focus on testing individual units.

See https://hg.mozilla.org/mozilla-central/rev/ed612eec41a44867a for an
example of how to add a simple test.

If you're converting an existing C++ unit test to a GTest, `this
commit <https://hg.mozilla.org/mozilla-central/rev/40740cddc131>`__ may
serve as a useful reference.


Setting prefs for a test
~~~~~~~~~~~~~~~~~~~~~~~~

If tests cover functionality that is disabled by default, you'll have to
change the relevant preferences either in the individual test:

::

   bool oldPref = Preferences::GetBool(prefKey);
   Preferences::SetBool(prefKey, true);
   … // test code
   Preferences::SetBool(prefKey, oldPref);

or, if it applies more broadly, the change can be applied to the whole
fixture (see `the GTest
docs <https://github.com/google/googletest/blob/master/googletest/README.md>`__,
or
`AutoInitializeImageLib <https://searchfox.org/mozilla-central/search?q=AutoInitializeImageLib%3A%3AAutoInitializeImageLib&path=>`__
as an example).


Adding a test to the build system
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Find a gtest directory appropriate for the module. If none exist create
a directory using the following convention: '<submodule>/tests/gtest'.
Create a moz.build file (in the newly created directory) with a module
declaration, replacing gfxtest with a unique name, and set
UNIFIED_SOURCES to contain all of the test file names.

What we're doing here is creating a list of source files that will be
compiled and linked only against the gtest version of libxul. This will
let these source files call internal xul symbols without making them
part of the binary we ship to users.

.. code-block::

   # -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
   # vim: set filetype=python:
   # This Source Code Form is subject to the terms of the Mozilla Public
   # License, v. 2.0. If a copy of the MPL was not distributed with this
   # file, you can obtain one at https://mozilla.org/MPL/2.0/.

   Library('gfxtest')

   UNIFIED_SOURCES = [
       <ListTestFiles>,
   ]

   FINAL_LIBRARY = 'xul-gtest'

Update '<submodule>/moz.build' in the parent directory to build your new
subdirectory in:

.. code-block:: python

   TEST_DIRS += [
       "gtest",
   ]

When adding tests to an existing moz.build file (it has FINAL_LIBRARY =
'xul-gtest'), add the following. That's it--there is no test manifest
required. Your tests will be automatically registered using a static
constructor.

.. code-block:: python

   UNIFIED_SOURCES = [
       'TestFoo.cpp',
   ]

Notes
~~~~~

The include file for the class you are testing may not need to be
globally exported, but it does need to be made available to the unit
test you are writing. In that case, add something like this to the
Makefile.in inside of the testing directory.

.. code-block:: python

    LOCAL_INCLUDES += [
        '/gfx/2d',
        '/gfx/2d/unittest',
        '/gfx/layers',
    ]

Gtests currently run from the test package under the **GTest** symbol on
`Treeherder <https://treeherder.mozilla.org/>`__ if you want to verify
that your test is working. Formerly they were run under the **B**
symbol, during \`make check`.


MozGTestBench
-------------

A Mozilla GTest Microbench is just a GTest that reports the test
duration to perfherder. It's an easy way to add low level performance
test. Keep in mind that there's a non-zero cost to monitoring
performance test so use them sparingly. You can still perform test
assertions.


Writing a Microbench GTest
~~~~~~~~~~~~~~~~~~~~~~~~~~

Use 'MOZ_GTEST_BENCH' instead of 'TEST' to time the execution of your
test. Example:

.. code-block:: cpp

   #include "gtest/MozGTestBench.h" // For MOZ_GTEST_BENCH

   ...

   MOZ_GTEST_BENCH(GfxBench, TEST_NAME, []{
     // Test to time the execution
   });

Make sure this file is registered with the file system using the
instructions above. If everything worked correctly you should see this
in the GTest log for your corresponding test:

.. code-block:: js

   PERFHERDER_DATA: {"framework": {"name": "platform_microbench"}, "suites": [{"name": "GfxBench", "subtests": [{"name": "CompositorSimpleTree", "value": 252674, "lowerIsBetter": true}]}]}


Sheriffing policy
~~~~~~~~~~~~~~~~~

Microbench tests measure the speed of a very specific operation. A
regression in a micro-benchmark may not lead to a user visible
regression and should not be treated as strictly as a Talos regression.
Large changes in microbench scores will also be expected when the code
is directly modified and should be accepted if the developer intended to
change that code. Micro-benchmarks however provide a framework for
adding performance tests for platform code and regression tests for
performance fixes. They will catch unintended regressions in code and
when correlated with a Talos regression might indicate the source of the
regression.
