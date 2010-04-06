/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff <jorendorff@mozilla.com>
 */

function onmessage(event) {
    var a = event.data.split(/\t/);
    var n = Number(a[0]);
    var workerDir = a[1];

    if (n <= 1) {
        postMessage("" + n);
    } else {
        var w1 = new Worker(workerDir + "worker-fib-child.js"),
            w2 = new Worker(workerDir + "worker-fib-child.js");
        var a = [];
        w1.onmessage = w2.onmessage = function(event) {
            a.push(+event.data);
            if (a.length == 2)
                postMessage(a[0] + a[1] + "");
        };
        w1.postMessage(n - 1 + "\t" + workerDir);
        w2.postMessage(n - 2 + "\t" + workerDir);
    }
}
