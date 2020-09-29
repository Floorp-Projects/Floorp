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
   eslint-plugin-mozilla/no-define-cc-etc
   eslint-plugin-mozilla/no-throw-cr-literal
   eslint-plugin-mozilla/no-useless-parameters
   eslint-plugin-mozilla/use-chromeutils-import

avoid-removeChild
-----------------

Rejects using element.parentNode.removeChild(element) when element.remove()
can be used instead.

balanced-listeners
------------------

Checks that for every occurrence of 'addEventListener' or 'on' there is an
occurrence of 'removeEventListener' or 'off' with the same event name.

consistent-if-bracing
---------------------

Checks that if/elseif/else bodies are braced consistently, so either all bodies
are braced or unbraced. Doesn't enforce either of those styles though.

import-browser-window-globals
-----------------------------

For scripts included in browser-window, this will automatically inject the
browser-window global scopes into the file.

import-content-task-globals
---------------------------

For files containing ContentTask.spawn calls, this will automatically declare
the frame script variables in the global scope. ContentTask is only available
to test files, so by default the configs only specify it for the mochitest based
configurations.

This saves setting the file as a mozilla/frame-script environment.

Note: due to the way ESLint works, it appears it is only easy to declare these
variables on a file global scope, rather than function global. This may mean that
they are incorrectly allowed, but given they are test files, this should be
detected during testing.

import-globals
--------------

Checks the filename of imported files e.g. ``Cu.import("some/path/Blah.jsm")``
adds Blah to the global scope.

Note: uses modules.json for a list of globals listed in each file.


import-globals-from
-------------------

Parses a file for globals defined in various unique Mozilla ways.

When a "import-globals-from <path>" comment is found in a file, then all globals
from the file at <path> will be imported in the current scope. This will also
operate recursively.

This is useful for scripts that are loaded as <script> tag in a window and rely
on each other's globals.

If <path> is a relative path, then it must be relative to the file being
checked by the rule.


import-headjs-globals
---------------------

Import globals from head.js and from any files that were imported by
head.js (as far as we can correctly resolve the path).

The following file import patterns are supported:

-  ``Services.scriptloader.loadSubScript(path)``
-  ``loader.loadSubScript(path)``
-  ``loadSubScript(path)``
-  ``loadHelperScript(path)``
-  ``import-globals-from path``

If path does not exist because it is generated e.g.
``testdir + "/somefile.js"`` we do our best to resolve it.

The following patterns are supported:

-  ``Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");``
-  ``loader.lazyImporter(this, "name1");``
-  ``loader.lazyRequireGetter(this, "name2"``
-  ``loader.lazyServiceGetter(this, "name3"``
-  ``XPCOMUtils.defineLazyModuleGetter(this, "setNamedTimeout", ...)``
-  ``loader.lazyGetter(this, "toolboxStrings"``
-  ``XPCOMUtils.defineLazyGetter(this, "clipboardHelper"``


mark-test-function-used
-----------------------

Simply marks `test` (the test method) or `run_test` as used when in mochitests
or xpcshell tests respectively. This avoids ESLint telling us that the function
is never called.


no-aArgs
--------

Checks that function argument names don't start with lowercase 'a' followed by
a capital letter. This is to prevent the use of Hungarian notation whereby the
first letter is a prefix that indicates the type or intended use of a variable.

no-compare-against-boolean-literals
-----------------------------------

Checks that boolean expressions do not compare against literal values
of `true` or `false`. This is to prevent overly verbose code such as
`if (isEnabled == true)` when `if (isEnabled)` would suffice.

no-useless-removeEventListener
------------------------------

Reject calls to removeEventListener where {once: true} could be used instead.

no-useless-run-test
-------------------

Designed for xpcshell-tests. Rejects definitions of ``run_test()`` where the
function only contains a single call to ``run_next_test()``. xpcshell's head.js
already defines a utility function so there is no need for duplication.

reject-importGlobalProperties
-----------------------------

Rejects calls to ``Cu.importGlobalProperties``.  Use of this function is
undesirable in some parts of the tree.


reject-some-requires
--------------------

This takes an option, a regular expression.  Invocations of
``require`` with a string literal argument are matched against this
regexp; and if it matches, the ``require`` use is flagged.


use-cc-etc
----------

This requires using ``Cc`` rather than ``Components.classes``, and the same for
``Components.interfaces``, ``Components.results`` and ``Components.utils``. This has
a slight performance advantage by avoiding the use of the dot.

use-default-preference-values
-----------------------------

Require providing a second parameter to get*Pref methods instead of
using a try/catch block.

use-ownerGlobal
---------------

Require .ownerGlobal instead of .ownerDocument.defaultView.

use-includes-instead-of-indexOf
-------------------------------

Use .includes instead of .indexOf to check if something is in an array or string.

use-returnValue
---------------

Warn when idempotent methods are called and their return value is unused.

use-services
------------

Requires the use of Services.jsm rather than Cc[].getService() where a service
is already defined in Services.jsm.

var-only-at-top-level
---------------------

Marks all var declarations that are not at the top level invalid.

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
