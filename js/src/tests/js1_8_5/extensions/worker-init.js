// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

if (typeof Worker != 'undefined') {
    JSTest.waitForExplicitFinish();
    // Messages sent during initialization are a corner case, but in any case
    // they should be delivered (no waiting until initialization is complete).
    var w = new Worker(workerDir + "worker-init-child.js"); // posts a message, then loops forever
    w.onmessage = function (event) {
        reportCompare(0, 0, "worker-init");
        JSTest.testFinished();
    };
} else {
    reportCompare(0, 0, "Test skipped. Shell workers required.");
}
