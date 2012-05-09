// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

if (typeof Worker != 'undefined') {
    JSTest.waitForExplicitFinish();
    var w = new Worker(workerDir + "worker-simple-child.js");
    var a = [];
    w.onmessage = function (event) {
        a.push(event.data);
        reportCompare(0, 0, "worker-simple");
        JSTest.testFinished();
    };
    w.postMessage("hello");
} else {
    reportCompare(0, 0, "Test skipped. Shell workers required.");
}
