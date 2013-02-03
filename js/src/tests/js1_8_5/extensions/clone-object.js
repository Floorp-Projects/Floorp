// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Assert that cloning b does the right thing as far as we can tell.
// Caveat: getters in b must produce the same value each time they're
// called. We may call them several times.
//
// If desc is provided, then the very first thing we do to b is clone it.
// (The self-modifying object test counts on this.)
//
function check(b, desc) {
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

function test() {
    check({});
    check([]);
    check({x: 0});
    check({x: 0.7, p: "forty-two", y: null, z: undefined});
    check(Array.prototype);
    check(Object.prototype);

    // before and after
    var b, a;

    // Slow array.
    b = [, 1, 2, 3];
    b.expando = true;
    b[5] = 5;
    b[0] = 0;
    b[4] = 4;
    delete b[2];
    check(b);

    // Check cloning properties other than basic data properties. (check()
    // asserts that the properties of the clone are configurable, writable,
    // enumerable data properties.)
    b = {};
    Object.defineProperties(b, {
        x: {enumerable: true, get: function () { return 12479; }},
        y: {enumerable: true, configurable: true, writable: false, value: 0},
        z: {enumerable: true, configurable: false, writable: true, value: 0},
        hidden: {enumerable:false, value: 1334}});
    check(b);

    // Check corner cases involving property names.
    b = {"-1": -1,
         0xffffffff: null,
         0x100000000: null,
         "": 0,
         "\xff\x7f\u7fff\uffff\ufeff\ufffe": 1, // random unicode id
         "\ud800 \udbff \udc00 \udfff": 2}; // busted surrogate pairs
    check(b);

    b = [];
    b[-1] = -1;
    b[0xffffffff] = null;
    b[0x100000000] = null;
    b[""] = 0;
    b["\xff\x7f\u7fff\uffff\ufeff\ufffe"] = 1;
    b["\ud800 \udbff \udc00 \udfff"] = 2;
    check(b);

    // An array's .length property is not enumerable, so it is not cloned.
    b = Array(5);
    assertEq(b.length, 5);
    a = check(b);
    assertEq(a.length, 0);

    b[1] = "ok";
    a = check(b);
    assertEq(a.length, 2);

    // Check that prototypes are not cloned, per spec.
    b = Object.create({x:1});
    b.y = 2;
    b.z = 3;
    check(b);

    // Check that cloning does not separate merge points in the tree.
    var same = {};
    b = {one: same, two: same};
    a = check(b);
    assertEq(a.one === a.two, true);

    b = [same, same];
    a = check(b);
    assertEq(a[0] === a[1], true);

    // Try cloning a deep object. Don't fail with "too much recursion".
    b = {};
    var current = b;
    for (var i = 0; i < 10000; i++) {
        var next = {};
        current['x' + i] = next;
        current = next;
    }
    check(b, "deepObject");  // takes 2 seconds :-\

    /*
      XXX TODO spin this out into its own test
    // This fails quickly with an OOM error. An exception would be nicer.
    function Infinitree() {
        return { get left() { return new Infinitree; },
                 get right() { return new Infinitree; }};
    }
    var threw = false;
    try {
        serialize(new Infinitree);
    } catch (exc) {
        threw = true;
    }
    assertEq(threw, true);
    */

    // Clone an array with holes.
    check([0, 1, 2, , 4, 5, 6]);

    // Array holes should not take up space.
    b = [];
    b[255] = 1;
    check(b);
    assertEq(serialize(b).length < 255, true);

    // Self-modifying object.
    // This should never read through to b's prototype.
    b = Object.create({y: 2}, 
                      {x: {enumerable: true,
                           configurable: true,
                           get: function() { if (this.hasOwnProperty("y")) delete this.y; return 1; }},
                       y: {enumerable: true,
                           configurable: true,
                           writable: true,
                           value: 3}});
    check(b, "selfModifyingObject");

    // Ignore properties with object-ids.
    var uri = "http://example.net";
    b = {x: 1, y: 2};
    Object.defineProperty(b, Array(uri, "x"), {enumerable: true, value: 3});
    Object.defineProperty(b, Array(uri, "y"), {enumerable: true, value: 5});
    check(b);
}

test();
reportCompare(0, 0, 'ok');
