// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

if (typeof Worker != 'undefined') {
    JSTest.waitForExplicitFinish();

    // This tests that a parent worker can terminate a child.  We run the test
    // several times serially. If terminate() doesn't work, the runaway Workers
    // will soon outnumber the number of threads in the thread pool, and we
    // will deadlock.
    var i = 0;

    function next() {
        if (++i == 10) {
            reportCompare(0, 0, "worker-terminate");
            JSTest.testFinished();
            return;
        }

        var w = new Worker(workerDir + "worker-terminate-child.js");
        w.onmessage = function (event) {
            reportCompare("killed", event.data, "killed runaway worker #" + i);
            next();
        };
        w.onerror = function (event) {
            reportCompare(0, 1, "Got error: " + event.message);
        };
        w.postMessage(workerDir);
    }
    next();
} else {
    reportCompare(0, 0, "Test skipped. Shell workers required.");
}
