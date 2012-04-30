JS Test Suite Readme
====================

The JS test suite is a fairly extensive collection of correctness and regression
tests for the Spidermonkey engine. Two harnesses run these tests: the shell test
harness in this directory and the "reftest" harness built into the browser, used
by Tinderbox. The browser reftests require additional manifest files; these are
generated automatically by the build phase 'package-tests'.

Adding a test
-------------
    Drop it in an appropriate directory under the tests directory.

        <fineprint> Some names are forbidden. Do not name your test browser.js,
        shell.js, jsref.js, template.js, user.js, js-test-driver-begin.js, or
        js-test-driver-end.js. </fineprint>

Adjusting when and how a test runs
----------------------------------
    Put a comment at the top of the header matching the format:
        // |reftest| <options> -- <comment>

    Where <options> is a standard reftest options string, as documented by:
        http://mxr.mozilla.org/mozilla-central/source/layout/tools/reftest/README.txt

    Example:
        // |reftest| skip-if(!xulRuntime.shell) -- does not always dismiss alert

        <fineprint> Either // or /* */ style comments may be used. The entire
        comment must appear in the first 512 bytes of the file. The control
        string must be in its own comment block. </fineprint>
