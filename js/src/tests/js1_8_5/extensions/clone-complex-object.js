// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function isClone(a, b) {
    var stack = [[a, b]];
    var memory = new WeakMap();
    var rmemory = new WeakMap();

    while (stack.length > 0) {
        var pair = stack.pop();
        var x = pair[0], y = pair[1];
        if (typeof x !== "object" || x === null) {
            // x is primitive.
            if (x !== y)
                return false;
        } else if (x instanceof Date) {
            if (x.getTime() !== y.getTime())
                return false;
        } else if (memory.has(x)) {
            // x is an object we have seen before in a.
            if (y !== memory.get(x))
                return false;
            assertEq(rmemory.get(y), x);
        } else {
            // x is an object we have not seen before.
	          // Check that we have not seen y before either.
            if (rmemory.has(y))
                return false;

            // x and y must be of the same [[Class]].
            var xcls = Object.prototype.toString.call(x);
            var ycls = Object.prototype.toString.call(y);
            if (xcls !== ycls)
                return false;

            // This function is only designed to check Objects and Arrays.
            assertEq(xcls === "[object Object]" || xcls === "[object Array]",
                     true);

            // Compare objects.
            var xk = Object.keys(x), yk = Object.keys(y);
            if (xk.length !== yk.length)
                return false;
            for (var i = 0; i < xk.length; i++) {
                // We must see the same property names in the same order.
                if (xk[i] !== yk[i])
                    return false;

                // Put the property values on the stack to compare later.
                stack.push([x[xk[i]], y[yk[i]]]);
            }

            // Record that we have seen this pair of objects.
            memory.set(x, y);
            rmemory.set(y, x);
        }
    }
    return true;
}

function check(a) {
    assertEq(isClone(a, deserialize(serialize(a))), true);
}

// Various recursive objects, i.e. those which the structured cloning
// algorithm wants us to reject due to "memory".
//
// Recursive array.
var a = [];
a[0] = a;
check(a);

// Recursive Object.
var b = {};
b.next = b;
check(b);

// Mutually recursive objects.
var a = [];
var b = {};
var c = {};
a[0] = b;
a[1] = b;
a[2] = b;
b.next = a;
check(a);
check(b);

// A date
check(new Date);

// A recursive object that is very large.
a = [];
b = a;
for (var i = 0; i < 10000; i++) {
    b[0] = {};
    b[1] = [];
    b = b[1];
}
b[0] = {owner: a};
b[1] = [];
check(a);

reportCompare(0, 0, 'ok');
