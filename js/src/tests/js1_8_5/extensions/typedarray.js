// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: js2; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Vladimir Vukicevic <vladimir@pobox.com>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 532774;
var summary = 'js typed arrays (webgl arrays)';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
if (typeof gczeal !== 'undefined')
    gczeal(0)

test();
//-----------------------------------------------------------------------------

function test()
{
    printBugNumber(BUGNUMBER);
    printStatus(summary);
    
    var TestPassCount = 0;
    var TestFailCount = 0;
    var TestTodoCount = 0;

    var TODO = 1;

    function check(fun, msg, todo) {
        var thrown = null;
        var success = false;
        try {
            success = fun();
        } catch (x) {
            thrown = x;
        }

        if (thrown)
            success = false;

        if (todo) {
            TestTodoCount++;

            if (success) {
                var ex = new Error;
                print ("=== TODO but PASSED? ===");
                print (ex.stack);
                print ("========================");
            }

            return;
        }

        if (success) {
            TestPassCount++;
        } else {
            TestFailCount++;

            var ex = new Error;
            print ("=== FAILED ===");
            if (msg)
                print (msg);
            print (ex.stack);
            if (thrown) {
                print ("    threw exception:");
                print (thrown);
            }
            print ("==============");
        }
    }

    function checkThrows(fun, type, todo) {
        var thrown = false;
        try {
            fun();
        } catch (x) {
            thrown = x;
        }

        if (typeof(type) !== 'undefined')
            if (thrown) {
                check(() => thrown instanceof type,
                      "expected " + type.name + " but saw " + thrown,
                      todo);
            } else {
                check(() => thrown, "expected " + type.name + " but no exception thrown", todo);
            }
        else
            check(() => thrown, undefined, todo);
    }

    function checkThrowsTODO(fun, type) {
        checkThrows(fun, type, true);
    }

    function testBufferManagement() {
        // Single buffer
        var buffer = new ArrayBuffer(128);
        buffer = null;
        gc();

        // Buffer with single view, kill the view first
        buffer = new ArrayBuffer(128);
        var v1 = new Uint8Array(buffer);
        gc();
        v1 = null;
        gc();
        buffer = null;
        gc();

        // Buffer with single view, kill the buffer first
        buffer = new ArrayBuffer(128);
        v1 = new Uint8Array(buffer);
        gc();
        buffer = null;
        gc();
        v1 = null;
        gc();

        // Buffer with multiple views, kill first view first
        buffer = new ArrayBuffer(128);
        v1 = new Uint8Array(buffer);
        v2 = new Uint8Array(buffer);
        gc();
        v1 = null;
        gc();
        v2 = null;
        gc();

        // Buffer with multiple views, kill second view first
        buffer = new ArrayBuffer(128);
        v1 = new Uint8Array(buffer);
        v2 = new Uint8Array(buffer);
        gc();
        v2 = null;
        gc();
        v1 = null;
        gc();

        // Buffer with multiple views, kill all possible subsets of views
        buffer = new ArrayBuffer(128);
        for (let order = 0; order < 16; order++) {
            var views = [ new Uint8Array(buffer),
                          new Uint8Array(buffer),
                          new Uint8Array(buffer),
                          new Uint8Array(buffer) ];
            gc();

            // Kill views according to the bits set in 'order'
            for (let i = 0; i < 4; i++) {
                if (order & (1 << i))
                    views[i] = null;
            }

            gc();

            views = null;
            gc();
        }

        // Similar: multiple views, kill them one at a time in every possible order
        buffer = new ArrayBuffer(128);
        for (let order = 0; order < 4*3*2*1; order++) {
            var views = [ new Uint8Array(buffer),
                          new Uint8Array(buffer),
                          new Uint8Array(buffer),
                          new Uint8Array(buffer) ];
            gc();

            var sequence = [ 0, 1, 2, 3 ];
            let groupsize = 4*3*2*1;
            let o = order;
            for (let i = 4; i > 0; i--) {
                groupsize = groupsize / i;
                let which = Math.floor(o/groupsize);
                [ sequence[i-1], sequence[which] ] = [ sequence[which], sequence[i-1] ];
                o = o % groupsize;
            }

            for (let i = 0; i < 4; i++) {
                views[i] = null;
                gc();
            }
        }

        // Multiple buffers with multiple views
        var views = [];
        for (let numViews of [ 1, 2, 0, 3, 2, 1 ]) {
            buffer = new ArrayBuffer(128);
            for (let viewNum = 0; viewNum < numViews; viewNum++) {
                views.push(new Int8Array(buffer));
            }
        }

        gcparam('markStackLimit', 200);
        var forceOverflow = [ buffer ];
        for (let i = 0; i < 1000; i++) {
            forceOverflow = [ forceOverflow ];
        }
        gc();
        buffer = null;
        views = null;
        gcslice(3); gcslice(3); gcslice(3); gcslice(3); gcslice(3); gcslice(3); gc();
    }

    var buf, buf2;

    buf = new ArrayBuffer(100);
    check(() => buf);
    check(() => buf.byteLength == 100);

    buf.byteLength = 50;
    check(() => buf.byteLength == 100);

    var zerobuf = new ArrayBuffer(0);
    check(() => zerobuf);
    check(() => zerobuf.byteLength == 0);

    check(() => (new Int32Array(zerobuf)).length == 0);
    checkThrows(() => new Int32Array(zerobuf, 1));

    var zerobuf2 = new ArrayBuffer();
    check(() => zerobuf2.byteLength == 0);

    checkThrows(() => new ArrayBuffer(-100), RangeError);
    // this is using js_ValueToECMAUInt32, which is giving 0 for "abc"
    checkThrowsTODO(() => new ArrayBuffer("abc"), TypeError);

    var zeroarray = new Int32Array(0);
    check(() => zeroarray.length == 0);
    check(() => zeroarray.byteLength == 0);
    check(() => zeroarray.buffer);
    check(() => zeroarray.buffer.byteLength == 0);

    var zeroarray2 = new Int32Array();
    check(() => zeroarray2.length == 0);
    check(() => zeroarray2.byteLength == 0);
    check(() => zeroarray2.buffer);
    check(() => zeroarray2.buffer.byteLength == 0);

    var a = new Int32Array(20);
    check(() => a);
    check(() => a.length == 20);
    check(() => a.byteLength == 80);
    check(() => a.byteOffset == 0);
    check(() => a.buffer);
    check(() => a.buffer.byteLength == 80);

    var b = new Uint8Array(a.buffer, 4, 4);
    check(() => b);
    check(() => b.length == 4);
    check(() => b.byteLength == 4);
    check(() => a.buffer == b.buffer);

    b[0] = 0xaa;
    b[1] = 0xbb;
    b[2] = 0xcc;
    b[3] = 0xdd;

    check(() => a[0] == 0);
    check(() => a[1] != 0);
    check(() => a[2] == 0);

    buf = new ArrayBuffer(4);
    check(() => (new Int8Array(buf)).length == 4);
    check(() => (new Uint8Array(buf)).length == 4);
    check(() => (new Int16Array(buf)).length == 2);
    check(() => (new Uint16Array(buf)).length == 2);
    check(() => (new Int32Array(buf)).length == 1);
    check(() => (new Uint32Array(buf)).length == 1);
    check(() => (new Float32Array(buf)).length == 1);
    checkThrows(() => (new Float64Array(buf)));
    buf2 = new ArrayBuffer(8);
    check(() => (new Float64Array(buf2)).length == 1);

    buf = new ArrayBuffer(5);
    check(() => buf);
    check(() => buf.byteLength == 5);

    check(() => new Int32Array(buf, 0, 1));
    checkThrows(() => new Int32Array(buf, 0));
    check(() => new Int8Array(buf, 0));

    check(() => (new Int8Array(buf, 3)).byteLength == 2);
    checkThrows(() => new Int8Array(buf, 500));
    checkThrows(() => new Int8Array(buf, 0, 50));
    checkThrows(() => new Float32Array(buf, 500));
    checkThrows(() => new Float32Array(buf, 0, 50));

    var sl = a.subarray(5,10);
    check(() => sl.length == 5);
    check(() => sl.buffer == a.buffer);
    check(() => sl.byteLength == 20);
    check(() => sl.byteOffset == 20);

    check(() => a.subarray(5,5).length == 0);
    check(() => a.subarray(-5).length == 5);
    check(() => a.subarray(-100).length == 20);
    check(() => a.subarray(0, 2).length == 2);
    check(() => a.subarray().length == a.length);
    check(() => a.subarray(-7,-5).length == 2);
    check(() => a.subarray(-5,-7).length == 0);
    check(() => a.subarray(15).length == 5);

    a = new Uint8Array([0xaa, 0xbb, 0xcc]);
    check(() => a.length == 3);
    check(() => a.byteLength == 3);
    check(() => a[1] == 0xbb);

    // not sure if this is supposed to throw or to treat "foo"" as 0.
    checkThrowsTODO(() => new Int32Array([0xaa, "foo", 0xbb]), Error);

    checkThrows(() => new Int32Array(-100));

    a = new Uint8Array(3);
    // XXX these are ignored now and return undefined
    //checkThrows(() => a[5000] = 0, RangeError);
    //checkThrows(() => a["hello"] = 0, TypeError);
    //checkThrows(() => a[-10] = 0, RangeError);
    check(() => (a[0] = "10") && (a[0] == 10));

    // check Uint8ClampedArray, which is an extension to this extension
    a = new Uint8ClampedArray(4);
    a[0] = 128;
    a[1] = 512;
    a[2] = -123.723;
    a[3] = "foopy";

    check(() => a[0] == 128);
    check(() => a[1] == 255);
    check(() => a[2] == 0);
    check(() => a[3] == 0);

    // check handling of holes and non-numeric values
    var x = Array(5);
    x[0] = "hello";
    x[1] = { };
    //x[2] is a hole
    x[3] = undefined;
    x[4] = true;

    a = new Uint8Array(x);
    check(() => a[0] == 0);
    check(() => a[1] == 0);
    check(() => a[2] == 0);
    check(() => a[3] == 0);
    check(() => a[4] == 1);

    a = new Float32Array(x);
    check(() => !(a[0] == a[0]));
    check(() => !(a[1] == a[1]));
    check(() => !(a[2] == a[2]));
    check(() => !(a[3] == a[3]));
    check(() => a[4] == 1);

    // test set()
    var empty = new Int32Array(0);
    a = new Int32Array(9);

    empty.set([]);
    empty.set([], 0);
    empty.set(empty);

    checkThrows(() => empty.set([1]));
    checkThrows(() => empty.set([1], 0));
    checkThrows(() => empty.set([1], 1));

    a.set([]);
    a.set([], 3);
    a.set([], 9);
    a.set(a);

    a.set(empty);
    a.set(empty, 3);
    a.set(empty, 9);
    a.set(Array.prototype);
    checkThrows(() => a.set(empty, 100));

    checkThrows(() => a.set([1,2,3,4,5,6,7,8,9,10]));
    checkThrows(() => a.set([1,2,3,4,5,6,7,8,9,10], 0));
    checkThrows(() => a.set([1,2,3,4,5,6,7,8,9,10], 0x7fffffff));
    checkThrows(() => a.set([1,2,3,4,5,6,7,8,9,10], 0xffffffff));
    checkThrows(() => a.set([1,2,3,4,5,6], 6));

    checkThrows(() => a.set(new Array(0x7fffffff)));
    checkThrows(() => a.set([1,2,3], 2147483647));

    a.set(ArrayBuffer.prototype);
    checkThrows(() => a.set(Int16Array.prototype), TypeError);
    checkThrows(() => a.set(Int32Array.prototype), TypeError);

    a.set([1,2,3]);
    a.set([4,5,6], 3);
    check(() =>
          a[0] == 1 && a[1] == 2 && a[2] == 3 &&
          a[3] == 4 && a[4] == 5 && a[5] == 6 &&
          a[6] == 0 && a[7] == 0 && a[8] == 0);

    b = new Float32Array([7,8,9]);
    a.set(b, 0);
    a.set(b, 3);
    check(() =>
          a[0] == 7 && a[1] == 8 && a[2] == 9 &&
          a[3] == 7 && a[4] == 8 && a[5] == 9 &&
          a[6] == 0 && a[7] == 0 && a[8] == 0);
    a.set(a.subarray(0,3), 6);
    check(() =>
          a[0] == 7 && a[1] == 8 && a[2] == 9 &&
          a[3] == 7 && a[4] == 8 && a[5] == 9 &&
          a[6] == 7 && a[7] == 8 && a[8] == 9);

    a.set([1,2,3,4,5,6,7,8,9]);
    a.set(a.subarray(0,6), 3);
    check(() =>
          a[0] == 1 && a[1] == 2 && a[2] == 3 &&
          a[3] == 1 && a[4] == 2 && a[5] == 3 &&
          a[6] == 4 && a[7] == 5 && a[8] == 6);

    a.set(a.subarray(3,9), 0);
    check(() =>
          a[0] == 1 && a[1] == 2 && a[2] == 3 &&
          a[3] == 4 && a[4] == 5 && a[5] == 6 &&
          a[6] == 4 && a[7] == 5 && a[8] == 6);

    // verify that subarray() returns a new view that
    // references the same buffer
    a.subarray(0,3).set(a.subarray(3,6), 0);
    check(() =>
          a[0] == 4 && a[1] == 5 && a[2] == 6 &&
          a[3] == 4 && a[4] == 5 && a[5] == 6 &&
          a[6] == 4 && a[7] == 5 && a[8] == 6);

    a = new ArrayBuffer(0x10);
    checkThrows(() => new Uint32Array(buffer, 4, 0x3FFFFFFF));

    check(() => new Float32Array(null).length === 0);

    a = new Uint8Array(0x100);
    b = Uint32Array.prototype.subarray.apply(a, [0, 0x100]);
    check(() => Object.prototype.toString.call(b) === "[object Uint8Array]");
    check(() => b.buffer === a.buffer);
    check(() => b.length === a.length);
    check(() => b.byteLength === a.byteLength);
    check(() => b.byteOffset === a.byteOffset);
    check(() => b.BYTES_PER_ELEMENT === a.BYTES_PER_ELEMENT);

    // webidl section 4.4.6, getter bullet point 2.2: prototypes are not
    // platform objects, and calling the getter of any attribute defined on the
    // interface should throw a TypeError according to
    checkThrows(() => ArrayBuffer.prototype.byteLength, TypeError);
    checkThrows(() => Int32Array.prototype.length, TypeError);
    checkThrows(() => Int32Array.prototype.byteLength, TypeError);
    checkThrows(() => Int32Array.prototype.byteOffset, TypeError);
    checkThrows(() => Float64Array.prototype.length, TypeError);
    checkThrows(() => Float64Array.prototype.byteLength, TypeError);
    checkThrows(() => Float64Array.prototype.byteOffset, TypeError);

    // webidl 4.4.6: a readonly attribute's setter is undefined. From
    // observation, that seems to mean it silently does nothing, and returns
    // the value that you tried to set it to.
    check(() => Int32Array.prototype.length = true);
    check(() => Float64Array.prototype.length = true);
    check(() => Int32Array.prototype.byteLength = true);
    check(() => Float64Array.prototype.byteLength = true);
    check(() => Int32Array.prototype.byteOffset = true);
    check(() => Float64Array.prototype.byteOffset = true);

    // ArrayBuffer, Int32Array and Float64Array are native functions and have a
    // .length, so none of these should throw:
    check(() => (new Int32Array(ArrayBuffer)).length >= 0);
    check(() => (new Int32Array(Int32Array)).length >= 0);
    check(() => (new Int32Array(Float64Array)).length >= 0);

    // webidl 4.4.6, under getters: "The value of the Function object’s
    // 'length' property is the Number value 0"
    //
    // Except this fails in getOwnPropertyDescriptor, I think because
    // Int32Array.prototype does not provide a lookup hook, and the fallback
    // case ends up calling the getter. Which seems odd to me, but much of this
    // stuff baffles me. It does seem strange that there's no way to do
    // getOwnPropertyDescriptor on any of these attributes.
    //
    //check(Object.getOwnPropertyDescriptor(Int32Array.prototype, 'byteOffset')['get'].length == 0);

    check(() => Int32Array.BYTES_PER_ELEMENT == 4);
    check(() => (new Int32Array(4)).BYTES_PER_ELEMENT == 4);
    check(() => (new Int32Array()).BYTES_PER_ELEMENT == 4);
    check(() => (new Int32Array(0)).BYTES_PER_ELEMENT == 4);
    check(() => Int16Array.BYTES_PER_ELEMENT == Uint16Array.BYTES_PER_ELEMENT);

    // test various types of args; Math.sqrt(4) is used to ensure that the
    // function gets a double, and not a demoted int
    check(() => (new Float32Array(Math.sqrt(4))).length == 2);
    check(() => (new Float32Array({ length: 10 })).length == 10);
    check(() => (new Float32Array({})).length == 0);
    check(() => new Float32Array("3").length === 3);
    check(() => new Float32Array(null).length === 0);
    check(() => new Float32Array(undefined).length === 0);

    // check that NaN conversions happen correctly with array conversions
    check(() => (new Int32Array([NaN])[0]) == 0);
    check(() => { var q = new Float32Array([NaN])[0]; return q != q; });

    // check that setting and reading arbitrary properties works
    // this is not something that will be done in real world
    // situations, but it should work when done just like in
    // regular objects
    buf = new ArrayBuffer(128);
    a = new Uint32Array(buf, 0, 4);
    check(() => a[0] ==  0 && a[1] == 0 && a[2] == 0 && a[3] == 0);
    buf.a = 42;
    buf.b = "abcdefgh";
    buf.c = {a:'literal'};
    check(() => a[0] ==  0 && a[1] == 0 && a[2] == 0 && a[3] == 0);

    check(() => buf.a == 42);
    delete buf.a;
    check(() => !buf.a);

    // check edge cases for small arrays
    // 16 reserved slots
    a = new Uint8Array(120);
    check(() => a.byteLength == 120);
    check(() => a.length == 120);
    for (var i = 0; i < a.length; i++)
        check(() => a[i] == 0)

    a = new Uint8Array(121);
    check(() => a.byteLength == 121);
    check(() => a.length == 121);
    for (var i = 0; i < a.length; i++)
        check(() => a[i] == 0)

    // check that TM generated byte offset is right (requires run with -j)
    a = new Uint8Array(100);
    a[99] = 5;
    b = new Uint8Array(a.buffer, 9); // force a offset
    // use a loop to invoke the TM
    for (var i = 0; i < b.length; i++)
        check(() => b[90] == 5)

    // Protos and proxies, oh my!
    var alien = newGlobal();

    var alien_view = alien.eval('view = new Uint8Array(7)');
    var alien_buffer = alien.eval('buffer = view.buffer');

    // when creating a view of a buffer in a different compartment, the view
    // itself should be created in the other compartment and wrapped for use in
    // this compartment. (There should never be a compartment boundary between
    // an ArrayBufferView and its ArrayBuffer.)
    var view = new Int8Array(alien_buffer);

    // First make sure they're looking at the same data
    alien_view[3] = 77;
    check(() => view[3] == 77);

    // Now check that the proxy setup is as expected
    check(() => isProxy(alien_view));
    check(() => isProxy(alien_buffer));
    check(() => isProxy(view)); // the real test

    // cross-compartment property access
    check(() => alien_buffer.byteLength == 7);
    check(() => alien_view.byteLength == 7);
    check(() => view.byteLength == 7);

    // typed array protos should be equal
    simple = new Int8Array(12);
    check(() => Object.getPrototypeOf(view) == Object.getPrototypeOf(simple));
    check(() => Object.getPrototypeOf(view) == Int8Array.prototype);

    // Most named properties are defined on %TypedArray%.prototype.
    check(() => !simple.hasOwnProperty('byteLength'));
    check(() => !Int8Array.prototype.hasOwnProperty('byteLength'));
    check(() => Object.getPrototypeOf(Int8Array.prototype).hasOwnProperty('byteLength'));

    check(() => !simple.hasOwnProperty("BYTES_PER_ELEMENT"));
    check(() => Int8Array.prototype.hasOwnProperty("BYTES_PER_ELEMENT"));
    check(() => !Object.getPrototypeOf(Int8Array.prototype).hasOwnProperty("BYTES_PER_ELEMENT"));

    // crazy as it sounds, the named properties are configurable per WebIDL.
    // But we are currently discussing the situation, and typed arrays may be
    // pulled into the ES spec, so for now this is disallowed.
    if (false) {
        check(() => simple.byteLength == 12);
        getter = Object.getOwnPropertyDescriptor(Int8Array.prototype, 'byteLength').get;
        Object.defineProperty(Int8Array.prototype, 'byteLength', { get: function () { return 1 + getter.apply(this) } });
        check(() => simple.byteLength == 13);
    }

    // test copyWithin()
    var numbers = [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ];

    function tastring(tarray) {
        return [...tarray].toString();
    }

    function checkCopyWithin(offset, start, end, dest, want) {
        var numbers_buffer = new Uint8Array(numbers).buffer;
        var view = new Int8Array(numbers_buffer, offset);
        view.copyWithin(dest, start, end);
        check(() => tastring(view) == want.toString());
        if (tastring(view) != want.toString()) {
            print("Wanted: " + want.toString());
            print("Got   : " + tastring(view));
        }
    }

    // basic copyWithin [2,5) -> 4
    checkCopyWithin(0, 2, 5, 4, [ 0, 1, 2, 3, 2, 3, 4, 7, 8 ]);

    // negative values should count from end
    checkCopyWithin(0, -7,  5,  4, [ 0, 1, 2, 3, 2, 3, 4, 7, 8 ]);
    checkCopyWithin(0,  2, -4,  4, [ 0, 1, 2, 3, 2, 3, 4, 7, 8 ]);
    checkCopyWithin(0,  2,  5, -5, [ 0, 1, 2, 3, 2, 3, 4, 7, 8 ]);
    checkCopyWithin(0, -7, -4, -5, [ 0, 1, 2, 3, 2, 3, 4, 7, 8 ]);

    // offset
    checkCopyWithin(2, 0, 3, 4, [ 2, 3, 4, 5, 2, 3, 4 ]);

    // clipping
    checkCopyWithin(0,  5000,  6000, 0, [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(0, -5000, -6000, 0, [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(0, -5000,  6000, 0, [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(0,  5000,  6000, 1, [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(0, -5000, -6000, 1, [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(0,  5000,  6000, 0, [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(2, -5000, -6000, 0, [ 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(2, -5000,  6000, 0, [ 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(2,  5000,  6000, 1, [ 2, 3, 4, 5, 6, 7, 8 ]);
    checkCopyWithin(2, -5000, -6000, 1, [ 2, 3, 4, 5, 6, 7, 8 ]);

    checkCopyWithin(2, -5000,    3, 1,     [ 2, 2, 3, 4, 6, 7, 8 ]);
    checkCopyWithin(2,     1, 6000, 0,     [ 3, 4, 5, 6, 7, 8, 8 ]);
    checkCopyWithin(2,     1, 6000, -4000, [ 3, 4, 5, 6, 7, 8, 8 ]);

    testBufferManagement();

    print ("done");

    reportCompare(0, TestFailCount, "typed array tests");
}
