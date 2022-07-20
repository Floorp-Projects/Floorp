Testing
=======
This documentation discusses how to write a test for the address bar and the
different test utilities that are useful when writing a test for the address
bar.

Common Tests
------------
Mochitests
~~~~~~~~~~
Some common tests for the address bar are the **mochitests**. The purpose of
a mochitest is to run the browser itself. Mochitests can be called
"browser tests", "mochitest-browser-chrome", or
"browser-chrome-mochitests". There are other types of mochitests that are not
for testing the browser and therefore can be ignored for the purpose of the
address bar. An example of a mochitest is
`tests/browser/browser_switchTab_currentTab.js <https://searchfox.org/mozilla-
central/source/browser/components/urlbar/tests/browser/browser_switchTab_
currentTab.js>`_

XPCShell
~~~~~~~~
`XPCShell Tests <https://firefox-source-docs.mozilla.org/testing/xpcshell/index
.html>`_ are another type of test relevant to the address bar. XPCShell tests
are often called unit tests because they tend to test specific modules or
components in isolation, as opposed the mochitest which have access to the full
browser chrome.

XPCShell tests do not use the browser UI and are completely separate from
browser chrome. XPCShell tests are executed in a JavaScript shell that is
outside of the browser. For historical context, the "XPC" naming convention is
from XPCOM (Cross Platform Component Model) which is an older framework that
allows programmers to write custom functions in one language, such as C++, and
connect it to other components in another language, such as JavaScript.

Each XPCShell test is executed in a new shell instance, therefore you will
see several Firefox icons pop up and close when XPCShell tests are executing.
These are two examples of XPCShell tests for the address bar
`test_providerHeuristicFallback <https://searchfox.org/mozilla-central/source
/browser/components/urlbar/tests/unit/test_providerHeuristicFallback.js>`_
and
`test_providerTabToSearch <https://searchfox.org/mozilla-central/source/browser
/components/urlbar/tests/unit/test_providerTabToSearch.js>`_.

When To Write a XPCShell or Mochitest?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Always default to writing an XPCShell test if it is possible. XPCShell
tests are faster to execute than browser tests. Although, most of the time you
will write a browser test because you could be modifying something in the UI or
testing a specific component in the UI.

If you are writing a test for a urlbarProvider, you can test the Provider
through a XPCShell test. Providers do not modify the UI, instead what they do is
receive a url string query, search for the string and bring back the result. An
example is the `ProviderPlaces <https://searchfox.org/mozilla-central/sou
rce/browser/components/urlbar/UrlbarProviderPlaces.jsm>`_, which fetches
results from the Places database. Another component that’s good for writing
XPCShell test is the `urlbarMuxer <https://searchfox.org/mozilla-central/
source/browser/components/urlbar/UrlbarMuxerUnifiedComplete.jsm>`_.

There may be times where writing both an XPCShell test and browser test is
necessary. In these situations, you could be testing the result from a Provider
and also testing what appears in the UI is correct.

How To Write a Test
-------------------

Test Boilerplate
~~~~~~~~~~~~~~~~
This basic test boilerplate includes a license code at the top and this license
code is present at the top of every test file, the ``"use strict"`` string is
to enable strict mode in JavaScript, and ``add_task`` function adds tests to be
executed by the test harness.

.. code-block:: javascript

   /* Any copyright is dedicated to the Public Domain.
    * http://creativecommons.org/publicdomain/zero/1.0/ */

   /**
    * This tests ensures that the urlbar ...
    */

   "use strict";

   add_task(async function testOne() {
     // testing code and assertions
   });

   add_task(async function testTwo() {
     // testing code and assertions
   });

In order to run a test use the ``./mach`` command, for example, ``./mach test <path to test
file>`` to run test locally. Use the command with ``--jsdebugger`` argument at
the end to open the DevTools debugger to step through the test, ``./mach test
<path to test> --jsdebugger``.

Manifest
~~~~~~~~
The manifest's purpose is to list all the test in the directory and dictate to
the test harness which files are test and how the test harness should run these
test. Anytime a test is created, the test file name needs to be added to the
manifest in alphabetical order.

Start in the manifest file and add your test name in alphabetical
order. The manifest file we should add our test in is
`browser.ini <https://searchfox.org/mozilla-central/source/browser/components/
urlbar/tests/browser/browser.ini>`_. The ``urlbar/test/browser/`` directory
is the main browser test directory for address bar, and the manifest file
linked above is the main browser test manifest.
The ``.ini`` file extension is an initialization file for Windows or MS-DOS.

Manifest Metadata
~~~~~~~~~~~~~~~~~
The manifest file can define common keys/metadata to influence the test's
behavior. For example, the metadata ``support-files`` are a list of additional
files required to run a test. Any values assigned to the key ``support-files``
only applies to the single file directly above the ``support-files`` key.
If more files require ``support-files``, then ``support-files`` need to be
added directly under the other test file names. Another example of a manifest
metadata is ``[DEFAULT]``. Anything under ``[DEFAULT]`` will be picked up by
all tests in the manifest file.

For information on all the manifest metadata available, please visit
:doc:`/build/buildsystem/test_manifests`.

Common Test Utilities
---------------------
This section describes common test utilities which may be useful when writing a
test for the address bar. Below are a description of common utils where you can
find helpful testing methods.

Many test utils modules end with ``TestUtils.jsm``. However not every testing
function will end with ``TestUtils.jsm``. For example, `PlacesUtils <https://
searchfox.org/mozilla-central/source/toolkit/components/places/PlacesUtils.
jsm>`_ does not have “Test” within its name.

A critical function to remember is the ``registerCleanupFunction`` within
the ``head.js`` file mentioned below. This function's purpose may be to clean
up the history or any other clean ups that are necessary after your test is
complete. Cleaning up after a browser test is necessary because clean up
ensures what is done within one test will not affect subsequent tests.

head.js and common-head.js
~~~~~~~~~~~~~~~~~~~~~~~~~~
The `head.js <https://searchfox.org/mozilla-central/source/browser/components
/urlbar/tests/browser/head.js>`_ file is executed at the beginning before each
test and contains imports to modules which are useful for each test.
Any tasks ``head.js`` adds (via add_task) will run first for each test, and
any variables and functions it defines will be available in the scope of
each test. This file is small because most of our Utils are actually in other
`.jsm` files.

The ``XPCOMUtils.defineLazyModuleGetters`` method within ``head.js`` sets up
modules names to where they can be found, their paths. ``Lazy`` means the files
are only imported if or when it is used. Any tests in this directory can use
these modules without importing it themselves in their own file.
The ``head.js`` provides a convenience for this purpose. The ``head.js`` file
imports `common-head.js <https://searchfox.org/mozilla-central/source/browser/components/urlbar/tests/browser/head-common.js>`_
making everything within ``head-common.js`` available in ``head.js`` as well.

The ``registerCleanupFunction`` is an important function in browser mochi tests
and it is part of the test harness. This function registers a callback function
to be executed when your test is complete. The purpose may be to clean up the
history or any other clean ups that are necessary after your test is complete.
For example, browser mochi tests are executed one after the other in the same
window instance. The global object in each test is the browser ``window``
object, for example, each test script runs in the browser window.
If the history is not cleaned up it will remain and may affect subsequent
browser tests. For most test outside of address bar, you may not need to clear
history. In addition to cleanup, ``head.js`` calls the
``registerCleanupFunction`` to ensure the urlbar panel is closed after each
test.

UrlbarTestUtils
~~~~~~~~~~~~~~~
`UrlbarTestUtils.jsm <https://searchfox.org/mozilla-central/source/browser/comp
onents/urlbar/tests/UrlbarTestUtils.jsm>`_ is useful for url bar testing. This
file contains methods that can help with starting a new search in the url bar,
waiting for a new search to complete, returning the results in
the view, and etc.

BrowserTestUtils
~~~~~~~~~~~~~~~~
`BrowserTestUtils.jsm <https://searchfox.org/mozilla-central/source/testing/moc
hitest/BrowserTestUtils/BrowserTestUtils.jsm>`_ is useful for browser window
testing. This file contains methods that can help with opening tabs, waiting
for certain events to happen in the window, opening new or private windows, and
etc.

TestUtils
~~~~~~~~~
`TestUtils.jsm <https://searchfox.org/mozilla-central/source/testing/modules/
TestUtils.jsm>`_ is useful for general purpose testing and does not depend on
the browser window. This file contains methods that are useful when waiting for
a condition to return true, waiting for a specific preference to change,
and etc.

PlacesTestUtils
~~~~~~~~~~~~~~~
`PlacesTestUtils.jsm <https://searchfox.org/mozilla-central/source/toolkit/comp
onents/places/tests/PlacesTestUtils.jsm>`_ is useful for adding visits, adding
bookmarks, waiting for notification of visited pages, and etc.

EventUtils
~~~~~~~~~~
`EventUtils.js <https://searchfox.org/mozilla-central/source/testing/mochitest
/tests/SimpleTest/EventUtils.js>`_ is an older test file and does not
need to be imported because it is not a ``.jsm`` file. ``EventUtils`` is only
used for browser tests, unlike the other TestUtils listed above which are
used for browser tests, XPCShell tests and other tests.

All the functions within ``EventUtils.js`` are automatically available in
browser tests. This file contains functions that are useful for synthesizing
mouse clicks and keypresses. Some commonly used functions are
``synthesizeMouseAtCenter`` which places the mouse at the center of the DOM
element and ``synthesizeKey`` which can be used to navigate the view and start
a search by using keydown and keyenter arguments.
