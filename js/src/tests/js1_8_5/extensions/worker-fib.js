/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

if (typeof Worker != 'undefined') {
    JSTest.waitForExplicitFinish();
    var w = Worker(workerDir + "worker-fib-child.js");
    w.onmessage = function (event) {
        reportCompare("21", event.data, "worker-fib");
        JSTest.testFinished();
    };
    w.postMessage("8\t" + workerDir); // 0 1 1 2 3 5 8 13 21
} else {
    reportCompare(0, 0, "Test skipped. Shell workers required.");
}
