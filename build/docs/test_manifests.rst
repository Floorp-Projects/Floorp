.. _test_manifests:

==============
Test Manifests
==============

Many test suites have their test metadata defined in files called
**test manifests**.

Test manifests are divided into two flavors: :ref:`manifest_destiny_manifests`
and :ref:`reftest_manifests`.

Naming Convention
=================

The build system does not enforce file naming for test manifest files.
However, the following convention is used.

mochitest.ini
   For the *plain* flavor of mochitests.

chrome.ini
   For the *chrome* flavor of mochitests.

browser.ini
   For the *browser chrome* flavor of mochitests.

a11y.ini
   For the *a11y* flavor of mochitests.

xpcshell.ini
   For *xpcshell* tests.

webapprt.ini
   For the *chrome* flavor of webapp runtime mochitests.

.. _manifest_destiny_manifests:

Manifest Destiny Manifests
==========================

Manifest destiny manifests are essentially ini files that conform to a basic
set of assumptions.

The `reference documentation <http://mozbase.readthedocs.org/en/latest/manifestdestiny.html>`_
for manifest destiny manifests describes the basic format of test manifests.

In summary, manifests are ini files with section names describing test files::

    [test_foo.js]
    [test_bar.js]

Keys under sections can hold metadata about each test::

    [test_foo.js]
    skip-if = os == "win"
    [test_foo.js]
    skip-if = os == "linux" && debug
    [test_baz.js]
    fail-if = os == "mac" || os == "android"

There is a special **DEFAULT** section whose keys/metadata apply to all
sections/tests::

    [DEFAULT]
    property = value

    [test_foo.js]

In the above example, **test_foo.js** inherits the metadata **property = value**
from the **DEFAULT** section.

Recognized Metadata
-------------------

Test manifests can define some common keys/metadata to influence behavior.
Those keys are as follows:

head
   List of files that will be executed before the test file. (Used in
   xpcshell tests.)

tail
   List of files that will be executed after the test file. (Used in
   xpcshell tests.)

support-files
   List of additional files required to run tests. This is typically
   defined in the **DEFAULT** section.

   Unlike other file lists, *support-files* supports a globbing mechanism
   to facilitate pulling in many files with minimal typing. This globbing
   mechanism is activated if an entry in this value contains a ``*``
   character. A single ``*`` will wildcard match all files in a directory.
   A double ``**`` will descend into child directories. For example,
   ``data/*`` will match ``data/foo`` but not ``data/subdir/bar`` where
   ``data/**`` will match ``data/foo`` and ``data/subdir/bar``.

   Support files starting with ``/`` are placed in a root directory, rather
   than a location determined by the manifest location. For mochitests,
   this allows for the placement of files at the server root. The source
   file is selected from the base name (e.g., ``foo`` for ``/path/foo``).
   Files starting with ``/`` cannot be selected using globbing.

generated-files
   List of files that are generated as part of the build and don't exist in
   the source tree.

   The build system assumes that each manifest file, test file, and file
   listed in **head**, **tail**, and **support-files** is static and
   provided by the source tree (and not automatically generated as part
   of the build). This variable tells the build system not to make this
   assumption.

   This variable will likely go away sometime once all generated files are
   accounted for in the build config.

   If a generated file is not listed in this key, a clobber build will
   likely fail.

dupe-manifest
   Record that this manifest duplicates another manifest.

   The common scenario is two manifest files will include a shared
   manifest file via the ``[include:file]`` special section. The build
   system enforces that each test file is only provided by a single
   manifest. Having this key present bypasses that check.

   The value of this key is ignored.

run-if
   Run this test only if the specified condition is true.
   See :ref:`manifest_filter_language`.

skip-if
   Skip this test if the specified condition is true.
   See :ref:`manifest_filter_language`.

fail-if
   Expect test failure if the specified condition is true.
   See :ref:`manifest_filter_language`.

run-sequentially
   If present, the test should not be run in parallel with other tests.

   Some test harnesses support parallel test execution on separate processes
   and/or threads (behavior varies by test harness). If this key is present,
   the test harness should not attempt to run this test in parallel with any
   other test.

   By convention, the value of this key is a string describing why the test
   can't be run in parallel.

.. _manifest_filter_language:

Manifest Filter Language
------------------------

Some manifest keys accept a special filter syntax as their values. These
values are essentially boolean expressions that are evaluated at test
execution time.

The expressions can reference a well-defined set of variables, such as
``os`` and ``debug``. These variables are populated from the
``mozinfo.json`` file. For the full list of available variables, see
the :ref:`mozinfo documentation <mozinfo_attributes>`.

See
`the source <https://hg.mozilla.org/mozilla-central/file/default/testing/mozbase/manifestdestiny/manifestparser/manifestparser.py>`_ for the full documentation of the
expression syntax until it is documented here.

.. todo::

   Document manifest filter language.

.. _manifest_file_installation:

File Installation
-----------------

Files referenced by manifests are automatically installed into the object
directory into paths defined in
:py:func:`mozbuild.frontend.emitter.TreeMetadataEmitter._process_test_manifest`.

Relative paths resolving to parent directory (e.g.
``support-files = ../foo.txt`` have special behavior.

For ``support-files``, the file will be installed to the default destination
for that manifest. Only the file's base name is used to construct the final
path: directories are irrelevant.  Files starting with ``/`` are an exception,
these are installed relative to the root of the destination; the base name is
instead used to select the file..

For all other entry types, the file installation is skipped.

.. _reftest_manifests:

Reftest Manifests
=================

See `MDN <https://developer.mozilla.org/en-US/docs/Creating_reftest-based_unit_tests>`_.
