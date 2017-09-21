// Prevent optimizing top-level
with ({}) { }


// Unboxed object constructor candidate
function Thing() {
    this.a = {};    // Object || null
    this.b = {};    // Object || null
}

(new Thing());
(new Thing()).a = null;
(new Thing()).b = null;


var arr = new Array(1000);
arr[0];

var ctx = new Thing();

function funPsh(t, x) {
    t.a = x;
}

function funBug(t, i) {
    t.b = t.a;      // GETPROP t.a
    t.a = null;     // SETPROP t.a
    arr[i] = 0;     // Bailout on uninitialized elements
    return t.b;
}

// Ion compile
for (var i = 0; i < 20000; ++i) {
    funBug(ctx, 0);
    funPsh(ctx, {});
}

// Invalidate
let tmp = { a: null, b: {} };
funBug(tmp, 0);

// Ion compile
for (var i = 0; i < 20000; ++i) {
    funBug(ctx, 0);
    funPsh(ctx, {});
}

// Trigger bailout
let res = funBug(ctx, 500);

// Result should not be clobbered by |t.a = null|
assertEq(res === null, false);
