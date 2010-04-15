/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

function onmessage(event) {
    var n = +event.data;
    if (n == 0)
        throw new Error("boom");
    var w = new Worker(workerDir + "worker-error-propagation-child.js");
    w.onmessage = function (event) { postMessage(event.data); };
    // No w.onerror here. We are testing error propagation when it is absent.
    w.postMessage(n - 1 + "");
}
