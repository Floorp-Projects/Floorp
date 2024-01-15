================================
Service Worker Performance Tests
================================

Our performance tests are mochitests running in the `mozperftest
<https://firefox-source-docs.mozilla.org/testing/perfdocs/mozperftest.html>`_
harness.  Tests reside under `dom/serviceworkers/test/performance
<https://searchfox.org/mozilla-central/source/dom/serviceworkers/test/performance>`_,
itemized in perftest.toml.  Beyond the standard mochitest machinery,
performance tests define a ``perfMetadata`` variable at the top level, and call:

``info("perfMetrics", JSON.stringify(metrics));``

to report their results, where *metrics* is a map from testpoint name to scalar
numeric value.  See the `performance scripts documentation
<https://firefox-source-docs.mozilla.org/testing/perfdocs/writing.html#mochitest>`_
for more.

They can be run via mach perftest, or as normal mochitests via mach test.
(Currently we can’t run the full manifest, see `bug 1865852
<https://bugzilla.mozilla.org/show_bug.cgi?id=1865852>`_.)

Adding new tests
================

Add files to `perftest.toml
<https://searchfox.org/mozilla-central/source/dom/serviceworkers/test/performance/perftest.toml>`_
as usual for mochitests.

Modify linux.yml, macosx.yml, and windows.yml under `taskcluster/ci/perftest
<https://searchfox.org/mozilla-central/source/taskcluster/ci/perftest>`_.
Currently, each test needs to be added individually to the run command (`here
<https://searchfox.org/mozilla-central/rev/91cc8848427fdbbeb324e6ca56a0d08d32d3c308/taskcluster/ci/perftest/linux.yml#121-149>`_,
for example).  kind.yml can be ignored–it provides some defaults.

Modify the documentation using:

``$ ./mach lint -l perfdocs . --fix --warnings --outgoing``

There's currently a `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=1872613>`_
which will likely cause the command to fail.  Running it a second time should
succeed.

Staging tests in try jobs
=========================

``$ ./mach try fuzzy --full``

Look for ``‘service-worker`` to find things like:

| >perftest-windows-service-worker
| >perftest-macosx-service-worker
| >perftest-linux-service-worker
|

Results
=======

Results can be found in treeherder on `mozilla-central
<https://treeherder.mozilla.org/jobs?repo=mozilla-central&searchStr=perftest>`_
and `autoland
<https://treeherder.mozilla.org/jobs?repo=autoland&searchStr=perftest>`_.  Look
for linux-sw, macosx-sw, and win-sw (`example
<https://treeherder.mozilla.org/perfherder/graphs?series=mozilla-central,4967140,1,15&selected=4967140,1814245176>`_).
These symbol names are defined in the .yml files under taskcluster/ci/perftest.

Contacts
========
| `Joshua Marshall <https://people.mozilla.org/p/jmarshall>`_   (DOM LWS)
| `Gregory Mierzwinski <https://people.mozilla.org/p/sparky>`_  (Performance Tools)
