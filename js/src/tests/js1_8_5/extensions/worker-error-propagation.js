/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

if (typeof Worker != 'undefined') {
    JSTest.waitForExplicitFinish();
    var w = Worker(workerDir + "worker-error-propagation-child.js");
    var errors = 0;
    w.onmessage = function () { throw new Error("no reply expected"); };
    w.onerror = function (event) {
        reportCompare("string", typeof event.message, "typeof event.message");
        JSTest.testFinished();
    };
    w.postMessage("5");
} else {
    reportCompare(0, 0, " PASSED! Test skipped. Shell workers required.");
}
