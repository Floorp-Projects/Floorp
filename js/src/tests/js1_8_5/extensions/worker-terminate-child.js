/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

onmessage = function (event) {
    var workerDir = event.message;
    var child = new Worker(workerDir + 'worker-terminate-iloop.js'); // loops forever
    child.terminate();
    postMessage("killed");
};
