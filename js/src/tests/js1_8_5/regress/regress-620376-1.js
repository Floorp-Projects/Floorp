/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Igor Bukanov
 */

function test() {
    if (typeof timeout != "function")
	return;

    var p = Proxy.create({ enumerate: function() { return Array(1e9); }});

    expectExitCode(6);
    timeout(0.001);

    var n = 0;
    for (i in p) { ++n;}
    return n;
}

test();
