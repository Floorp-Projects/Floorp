// |reftest| skip-if(!xulRuntime.shell||Android)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

if (typeof timeout == 'function' && typeof Worker != 'undefined') {
    // We don't actually ever call JSTest.testFinished(); instead we
    // time out and exit the shell with exit code 6.
    JSTest.waitForExplicitFinish();
    expectExitCode(6);
    timeout(1.0);
    for (var i = 0; i < 5; i++)
        new Worker(workerDir + "worker-timeout-child.js"); // just loops forever
} else {
    reportCompare(0, 0, "Test skipped. Shell workers and timeout required.");
}
