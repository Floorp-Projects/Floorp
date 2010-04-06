/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

if (typeof Worker != 'undefined') {
    JSTest.waitForExplicitFinish();

    // The script throws new Error("fail") on first message.
    var w = Worker(workerDir + "worker-error-child.js");
    var a = [];
    w.onerror = function (event) {
        reportCompare("fail", event.message, "worker-error");
        JSTest.testFinished();
    };
    w.postMessage("hello");
} else {
    reportCompare(0, 0, "Test skipped. Shell workers required.");
}
