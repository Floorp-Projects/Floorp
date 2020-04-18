Static analysis
===============

This document is split in two parts. The first part will focus on the
modern and robust way of static-analysis and the second part will
present the build-time static-analysis.

For linting, please see the `linting documentation </code-quality/lint/>`_.

For reviews, use the `#static-analysis-reviewers review group <https://phabricator.services.mozilla.com/project/view/120/>`__.
Ask questions on `#static-analysis:mozilla.org <https://chat.mozilla.org/#/room/#static-analysis:mozilla.org>`__.


Clang-Tidy static analysis
--------------------------

Our current static-analysis infrastructure is based on
`clang-tidy <http://clang.llvm.org/extra/clang-tidy/>`__. It uses
checkers in order to assert different programming errors present in the
code. The checkers that we use are split into 3 categories:

#. `Firefox specific checkers <https://searchfox.org/mozilla-central/source/build/clang-plugin>`_. They detect incorrect Gecko programming
   patterns which could lead to bugs or security issues.
#. `Clang-tidy checkers <https://clang.llvm.org/extra/clang-tidy/checks/list.html>`_. They aim to suggest better programming practices
   and to improve memory efficiency and performance.
#. `Clang-analyzer checkers <https://clang-analyzer.llvm.org/>`_. These checks are more advanced, for example
   some of them can detect dead code or memory leaks, but as a typical
   side effect they have false positives. Because of that, we have
   disabled them for now, but will enable some of them in the near
   future.

In order to simplify the process of static-analysis we have focused on
integrating this process with Phabricator and mach. A list of some
checkers that are used during automated scan can be found
`here <https://searchfox.org/mozilla-central/source/tools/clang-tidy/config.yaml>`__.

We are also running Coverity at review phase and a few times per day
(when autoland is merged into mozilla-central).

Static analysis at review phase
-------------------------------

We created a TaskCluster bot that runs clang static analysis on every
patch submitted to Phabricator. It then quickly reports any code defects
directly on the review platform, thus preventing bad patches from
landing until all their defects are fixed. Currently, its feedback is
posted in about 10 minutes after a patch series is published on the
review platform.

As part of the process, the various linting jobs are also executed
using try. This can be also used to add new jobs, see: :ref:`attach-job-review`.
An example of automated review can be found `on
phabricator <https://phabricator.services.mozilla.com/D2066>`__.


Mach static analysis
--------------------

It is supported on all Firefox built platforms. During the first run it
automatically installs all of its dependencies like clang-tidy
executable in the .mozbuild folder thus making it very easy to use. The
resources that are used are provided by toolchain artifacts clang-tidy
target.

This is used through ``mach static-analysis`` command that has the
following parameters:

-  ``check`` - Runs the checks using the installed helper tool from
   ~/.mozbuild.
-  ``--checks, -c`` - Checks to enabled during the scan. The checks
   enabled
   `in the yaml file <https://searchfox.org/mozilla-central/source/tools/clang-tidy/config.yaml>`__
   are used by default.
-  ``--fix, -f`` - Try to autofix errors detected by the checkers.
   Depending on the checker, this option might not do anything.
   The list of checkers with autofix can be found on the `clang-tidy website <https://clang.llvm.org/extra/clang-tidy/checks/list.html>`__.
-  ``--header-filter, -h-f`` - Regular expression matching the names of
   the headers to output diagnostic from.Diagnostic from the main file
   of each translation unit are always displayed.

As an example we  run static-analysis through mach on
``dom/presentation/Presentation.cpp`` with
``google-readability-braces-around-statements`` check and autofix we
would have:

.. code-block:: shell

   ./mach static-analysis check --checks="-*, google-readability-braces-around-statements" --fix dom/presentation/Presentation.cpp

If you want to use a custom clang-tidy binary this can be done by using
the ``install`` subcommand of ``mach static-analysis``, but please note
that the archive that is going to be used must be compatible with the
directory structure clang-tidy from toolchain artifacts.

.. code-block:: shell

   ./mach static-analysis install clang.tar.gz


Regression Testing
------------------

In order to prevent regressions in our clang-tidy based static analysis,
we have created a
`task <https://searchfox.org/mozilla-central/source/taskcluster/ci/static-analysis-autotest/kind.yml>`__
on automation. This task runs on each commit and launches a test suite
that is integrated into mach.

The test suite implements the following:

-  Downloads the necessary clang-tidy artifacts.
-  Reads the
   `configuration <https://searchfox.org/mozilla-central/source/tools/clang-tidy/config.yaml>`__
   file.
-  For each checker reads the test file plus the expected result. A
   sample of test and expected result can be found
   `in the test file <https://searchfox.org/mozilla-central/source/tools/clang-tidy/test/clang-analyzer-deadcode.DeadStores.cpp>`__
   and
   `the json file <https://searchfox.org/mozilla-central/source/tools/clang-tidy/test/clang-analyzer-deadcode.DeadStores.json>`__.

This testing suit can be run locally by doing the following:

.. code-block:: shell

   ./mach static-analysis autotest

If we want to test only a specific checker, let's say
modernize-raw-string-literal, we can run:

.. code-block:: shell

   ./mach static-analysis autotest modernize-raw-string-literal

If we want to add a new checker we need to generated the expected result
file, by doing:

.. code-block:: shell

   ./mach static-analysis autotest modernize-raw-string-literal -d


Build-time static-analysis
--------------------------

If you want to build with the Firefox Clang plug-in
(located in ``/build/clang-plugin`` and associated with
``MOZ_CLANG_PLUGIN`` and the attributes in ``/mfbt/Attributes.h``)
just add ``--enable-clang-plugin`` to your mozconfig!
If you want to also have our experimental checkers that will produce ``warnings`` as
diagnostic messages also add ``--enable-clang-plugin-alpha``.
This requires to build Firefox using Clang.

Configuring the build environment
---------------------------------

Once you have your Clang build in place, you will need to set up tools
to use it.
A full working .mozconfig for the desktop browser is:

.. code-block:: shell

   . $topsrcdir/browser/config/mozconfig
   mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-ff-dbg

   ac_add_options --enable-debug

Attempts to use ``ccache`` will likely result in failure to compile. It
is also necessary to avoid optimized builds, as these will modify macros
which will result in many false positives.

At this point, your Firefox build environment should be configured to
compile via the Clang static analyzer!


Performing scanning builds
--------------------------

It is not enough to simply start the build like normal. Instead, you
need to run the build through a Clang utility script which will keep
track of all produced analysis and consolidate it automatically.

Reports are published daily on
`https://sylvestre.ledru.info/reports/fx-scan-build/ <http://sylvestre.ledru.info/reports/fx-scan-build/>`__
Many of the defects reported as sources for Good First Bug.

That script is scan-build. You can find it in
``$clang_source/tools/scan-build/scan-build``.

Try running your build through ``scan-build``:

.. code-block:: shell

   $ cd /path/to/mozilla/source

   # Blow away your object directory because incremental builds don't make sense
   $ rm -rf obj-dir

   # To start the build:
   scan-build --show-description ./mach build -v

   # The above should execute without any errors. However, it should take longer than
   # normal because all compilation will be executing through Clang's static analyzer,
   # which adds overhead.

If things are working properly, you should see a bunch of console spew,
just like any build.

The first time you run scan-build, CTRL+C after a few files are
compiled. You should see output like:

.. code-block:: shell

   scan-build: 3 bugs found.
   scan-build: Run 'scan-view /Users/gps/tmp/mcsb/2011-12-15-3' to examine bug reports.

If you see a message like:

.. code-block:: shell

   scan-build: Removing directory '/var/folders/s2/zc78dpsx2rz6cpc_21r9g5hr0000gn/T/scan-build-2011-12-15-1' because it contains no reports.

either no static analysis results were available yet or your environment
is not configured properly.

By default, ``scan-build`` writes results to a folder in a
pseudo-temporary location. You can control where results go by passing
the ``-o /path/to/output`` arguments to ``scan-build``.

You may also want to run ``scan-build --help`` to see all the options
available. For example, it is possible to selectively enable and disable
individual analyzers.


Analyzing the output
--------------------

Once the build has completed, ``scan-build`` will produce a report
summarizing all the findings. This is called ``index.html`` in the
output directory. You can run ``scan-view`` (from
``$clang_source/tools/scan-view/scan-view``) as ``scan-build's`` output
suggests; this merely fires up a local HTTP server. Or you should be
able to open the ``index.html`` directly with your browser.


False positives
---------------

By definition, there are currently false positives in the static
analyzer. A lot of these are due to the analyzer having difficulties
following the relatively complicated error handling in various
preprocessor macros.

.. See also
   --------
   -  `Configuring Build Options </en/Configuring_Build_Options>`__
   -  `Developer Guide </En/Developer_Guide>`__
