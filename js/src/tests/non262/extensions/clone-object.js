// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function test() {
    var check = clone_object_check;

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

    // Check that array's .length property is cloned.
    b = Array(5);
    assertEq(b.length, 5);
    a = check(b);
    assertEq(a.length, 5);

    b = Array(0);
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
    assertEq(serialize(b).clonebuffer.length < 255, true);

    // Check that trailing holes in an array are preserved.
    b = [1,2,3,,];
    assertEq(b.length, 4);
    a = check(b);
    assertEq(a.length, 4);
    assertEq(a.toString(), "1,2,3,");

    b = [1,2,3,,,];
    assertEq(b.length, 5);
    a = check(b);
    assertEq(a.length, 5);
    assertEq(a.toString(), "1,2,3,,");

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
