JS Test Suite Readme
====================

The JS test suite is a fairly extensive collection of correctness and regression
tests for the Spidermonkey engine. Two harnesses run these tests: the shell test
harness in this directory and the "reftest" harness built into the browser, used
by Tinderbox. The browser reftests require additional manifest files; these are
generated automatically by the build phase 'package-tests' using the
'--make-manifests' option to jstests.py.

Creating a test
---------------
For general information, see
https://developer.mozilla.org/en-US/docs/SpiderMonkey/Creating_JavaScript_tests

Adding a test
-------------
    Drop it in an appropriate directory under the tests directory.

        <fineprint> Some names are forbidden. Do not name your test browser.js,
        shell.js, jsref.js, template.js, user.js, js-test-driver-begin.js, or
        js-test-driver-end.js, or any of the names of the files in supporting/.
        </fineprint>

Adjusting when and how a test runs
----------------------------------
    Put a comment at the top of the header matching the format:
        // |reftest| <failure-type> -- <comment>

    Where <failure-type> is a standard reftest <failure-type> string, as documented by:
        http://mxr.mozilla.org/mozilla-central/source/layout/tools/reftest/README.txt

    Example:
        // |reftest| skip-if(!xulRuntime.shell) -- does not always dismiss alert

        <fineprint> Either // or /* */ style comments may be used. The entire
        comment must appear in the first 512 bytes of the file. The control
        string must be in its own comment block. </fineprint>

    When adding such comments to individual files is not feasible (e.g., for
    imported tests), reftest manifest entries can be added to jstests.list
    instead. Combining in-file comments with entries in this manifest file for
    the same files is not supported (the one from the manifest file will be
    used). Only the following two forms are supported:
        <failure-type> include <relative_path>
        <failure-type> script <relative_path>
    The <type> "include" indicates that <failure-type> should apply to all test
    cases within a directory. A statement for a nested directory or script
    overrides one for an enclosing directory.

Running tests
-------------
See
https://developer.mozilla.org/en-US/docs/SpiderMonkey/Running_Automated_JavaScript_Tests
