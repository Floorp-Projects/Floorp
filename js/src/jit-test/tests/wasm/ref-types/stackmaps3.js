// Generates a bunch of numbers-on-the-heap, and tries to ensure that they are
// held live -- at least for a short while -- only by references from the wasm
// evaluation stack.  Then assembles them in a list and checks that the list
// is as expected (and we don't segfault).  While all this is running we also
// have an regular interrupt whose handler does a bunch of allocation, so as
// to cause as much disruption as possible.

// Note this makes an assumption about how the wasm compiler works.  There's
// no particular reason that the wasm compiler needs to keep the results of
// the $mkBoxedInt calls on the machine stack.  It could equally cache them in
// registers or even reorder the call sequences so as to interleave
// construction of the list elements with construction of the list itself.  It
// just happens that our baseline compiler will behave as described.  That
// said, however, it's hard to imagine how an implementation could complete
// the list construction without having at least one root in a register or on
// the stack, so the test still has value regardless of how the underlying
// implementation works.

const {Module,Instance} = WebAssembly;

const DEBUG = false;

let t =
  `(module
     (import "" "mkCons" (func $mkCons (param externref) (param externref) (result externref)))
     (import "" "mkBoxedInt" (func $mkBoxedInt (result externref)))

     (func $mkNil (result externref)
       ref.null extern
     )

     (func $mkConsIgnoringScalar
              (param $hd externref) (param i32) (param $tl externref)
              (result externref)
        (local.get $hd)
        (local.get $tl)
        call $mkCons
     )

     (func $mkList (export "mkList") (result externref)
        call $mkList20
     )

     (func $mkList20 (result externref)
       ;; create 20 pointers to boxed ints on the stack, plus a few
       ;; scalars for added confusion
       (local $scalar99 i32)
       (local $scalar97 i32)
       (local.set $scalar99 (i32.const 99))
       (local.set $scalar97 (i32.const 97))

       call $mkBoxedInt
       local.get $scalar99
       call $mkBoxedInt
       call $mkBoxedInt
       local.get $scalar97
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkBoxedInt
       call $mkNil
       ;; Now we have (pointers to) 20 boxed ints and a NIL on the stack, and
       ;; nothing else holding them live.  Build a list from the elements.
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkCons
       call $mkConsIgnoringScalar
       call $mkCons
       call $mkConsIgnoringScalar
     )
   )`;

let boxedIntCounter = 0;

function BoxedInt() {
    this.theInt = boxedIntCounter;
    boxedIntCounter++;
}

function mkBoxedInt() {
    return new BoxedInt();
}

function printBoxedInt(bi) {
    print(bi.theInt);
}

function Cons(hd, tl) {
    this.hd = hd;
    this.tl = tl;
}

function mkCons(hd, tl) {
    return new Cons(hd, tl);
}

function showList(list) {
    print("[");
    while (list) {
        printBoxedInt(list.hd);
        print(",");
        list = list.tl;
    }
    print("]");
}

function checkList(list, expectedHdValue, expectedLength) {
    while (list) {
        if (expectedLength <= 0)
            return false;
        if (list.hd.theInt !== expectedHdValue) {
            return false;
        }
        list = list.tl;
        expectedHdValue++;
        expectedLength--;
    }
    if (expectedLength == 0) {
        return true;
    } else {
        return false;
    }
}

let i = wasmEvalText(t, {"":{mkCons: mkCons, mkBoxedInt: mkBoxedInt}});


function Croissant(chocolate) {
    this.chocolate = chocolate;
}

function allocates() {
    return new Croissant(true);
}

function handler() {
    if (DEBUG) {
        print('XXXXXXXX icallback: START');
    }
    let q = allocates();
    let sum = 0;
    for (let i = 0; i < 15000; i++) {
        let x = allocates();
        // Without this hoop jumping to create an apparent use of |x|, Ion
        // will remove the allocation call and make the test pointless.
        if (x == q) { sum++; }
    }
    // Artificial use of |sum|.  See comment above.
    if (sum == 133713371337) { print("unlikely!"); }
    timeout(1, handler);
    if (DEBUG) {
        print('XXXXXXXX icallback: END');
    }
    return true;
}

timeout(1, handler);

for (let n = 0; n < 10000; n++) {
    let listLowest = boxedIntCounter;

    // Create the list in wasm land, possibly inducing GC on the way
    let aList = i.exports.mkList();

    // Check it is as we expect
    let ok = checkList(aList, listLowest, 20/*expected length*/);
    if (!ok) {
        print("Failed on list: ");
        showList(aList);
    }
    assertEq(ok, true);
}

// If we get here, the test finished successfully.
