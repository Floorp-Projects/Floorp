/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


// The Worker constructor can take a relative URL, but different test runners
// run in different enough environments that it doesn't all just automatically
// work. For the shell, we use just a filename; for the browser, see browser.js.
var workerDir = '';

// explicitly turn on js185
// XXX: The browser currently only supports up to version 1.8
if (typeof version != 'undefined')
{
  version(185);
}

// Assert that cloning b does the right thing as far as we can tell.
// Caveat: getters in b must produce the same value each time they're
// called. We may call them several times.
//
// If desc is provided, then the very first thing we do to b is clone it.
// (The self-modifying object test counts on this.)
//
function clone_object_check(b, desc) {
    function classOf(obj) {
        return Object.prototype.toString.call(obj);
    }

    function ownProperties(obj) {
        return Object.getOwnPropertyNames(obj).
            map(function (p) { return [p, Object.getOwnPropertyDescriptor(obj, p)]; });
    }

    function isCloneable(pair) {
        return typeof pair[0] === 'string' && pair[1].enumerable;
    }

    function notIndex(p) {
        var u = p >>> 0;
        return !("" + u == p && u != 0xffffffff);
    }

    function assertIsCloneOf(a, b, path) {
        assertEq(a === b, false);

        var ca = classOf(a);
        assertEq(ca, classOf(b), path);

        assertEq(Object.getPrototypeOf(a),
                 ca == "[object Object]" ? Object.prototype : Array.prototype,
                 path);

        // 'b', the original object, may have non-enumerable or XMLName
        // properties; ignore them.  'a', the clone, should not have any
        // non-enumerable properties (except .length, if it's an Array) or
        // XMLName properties.
        var pb = ownProperties(b).filter(isCloneable);
        var pa = ownProperties(a);
        for (var i = 0; i < pa.length; i++) {
            assertEq(typeof pa[i][0], "string", "clone should not have E4X properties " + path);
            if (!pa[i][1].enumerable) {
                if (Array.isArray(a) && pa[i][0] == "length") {
                    // remove it so that the comparisons below will work
                    pa.splice(i, 1);
                    i--;
                } else {
                    throw new Error("non-enumerable clone property " + uneval(pa[i][0]) + " " + path);
                }
            }
        }

        // Check that, apart from properties whose names are array indexes, 
        // the enumerable properties appear in the same order.
        var aNames = pa.map(function (pair) { return pair[1]; }).filter(notIndex);
        var bNames = pa.map(function (pair) { return pair[1]; }).filter(notIndex);
        assertEq(aNames.join(","), bNames.join(","), path);

        // Check that the lists are the same when including array indexes.
        function byName(a, b) { a = a[0]; b = b[0]; return a < b ? -1 : a === b ? 0 : 1; }
        pa.sort(byName);
        pb.sort(byName);
        assertEq(pa.length, pb.length, "should see the same number of properties " + path);
        for (var i = 0; i < pa.length; i++) {
            var aName = pa[i][0];
            var bName = pb[i][0];
            assertEq(aName, bName, path);

            var path2 = path + "." + aName;
            var da = pa[i][1];
            var db = pb[i][1];
            assertEq(da.configurable, true, path2);
            assertEq(da.writable, true, path2);
            assertEq("value" in da, true, path2);
            var va = da.value;
            var vb = b[pb[i][0]];
            if (typeof va === "object" && va !== null)
                queue.push([va, vb, path2]);
            else
                assertEq(va, vb, path2);
        }
    }

    var banner = "while testing clone of " + (desc || uneval(b));
    var a = deserialize(serialize(b));
    var queue = [[a, b, banner]];
    while (queue.length) {
        var triple = queue.shift();
        assertIsCloneOf(triple[0], triple[1], triple[2]);
    }

    return a; // for further testing
}
