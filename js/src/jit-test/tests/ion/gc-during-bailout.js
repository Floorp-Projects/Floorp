setJitCompilerOption("ion.warmup.trigger", 30);
gcPreserveCode();
var o = {};

function f(i) {
    var obj_1 = { a: 0 };
    var obj_2 = { a: 0 };
    var obj_3 = { a: 0 };
    var obj_4 = { a: 0 };
    var obj_5 = { a: 0 };
    var obj_6 = { a: 0 };
    var obj_7 = { a: 0 };
    var obj_8 = { a: 0 };
    var obj_9 = { a: 0 };
    var obj_10 = { a: 0 };
    var obj_11 = { a: 0 };
    var obj_12 = { a: 0 };
    var obj_13 = { a: 0 };
    var obj_14 = { a: 0 };
    var obj_15 = { a: 0 };
    var obj_16 = { a: 0 };
    var obj_17 = { a: 0 };
    var obj_18 = { a: 0 };
    var obj_19 = { a: 0 };
    var obj_20 = { a: 0 };
    var obj_21 = { a: 0 };
    var obj_22 = { a: 0 };
    var obj_23 = { a: 0 };
    var obj_24 = { a: 0 };
    var obj_25 = { a: 0 };
    var obj_26 = { a: 0 };
    var obj_27 = { a: 0 };
    var obj_28 = { a: 0 };
    var obj_29 = { a: 0 };
    var obj_30 = { a: 0 };

    // Doing a bailout after the return of the function call implies that we
    // cannot resume before the function call.  Thus, we would have to recover
    // the 30 objects allocations during the bailout.
    schedulegc(i % 100);
    bailout();

    obj_1.a = 1;
    obj_2.a = 1;
    obj_3.a = 1;
    obj_4.a = 1;
    obj_5.a = 1;
    obj_6.a = 1;
    obj_7.a = 1;
    obj_8.a = 1;
    obj_9.a = 1;
    obj_10.a = 1;
    obj_11.a = 1;
    obj_12.a = 1;
    obj_13.a = 1;
    obj_14.a = 1;
    obj_15.a = 1;
    obj_16.a = 1;
    obj_17.a = 1;
    obj_18.a = 1;
    obj_19.a = 1;
    obj_20.a = 1;
    obj_21.a = 1;
    obj_22.a = 1;
    obj_23.a = 1;
    obj_24.a = 1;
    obj_25.a = 1;
    obj_26.a = 1;
    obj_27.a = 1;
    obj_28.a = 1;
    obj_29.a = 1;
    obj_30.a = 1;
}

for (var i = 0; i < 200; i++) {
    // Do not inline 'f', to keep re-entering 'f' at every loop iteration.
    with (o) { }
    f(i);
}

// This is the same test except that we do not preserve code under shrinking GC.
gczeal(14);

function g(i) {
    var obj_1 = { a: 0 };
    var obj_2 = { a: 0 };
    var obj_3 = { a: 0 };
    var obj_4 = { a: 0 };
    var obj_5 = { a: 0 };
    var obj_6 = { a: 0 };
    var obj_7 = { a: 0 };
    var obj_8 = { a: 0 };
    var obj_9 = { a: 0 };
    var obj_10 = { a: 0 };
    var obj_11 = { a: 0 };
    var obj_12 = { a: 0 };
    var obj_13 = { a: 0 };
    var obj_14 = { a: 0 };
    var obj_15 = { a: 0 };
    var obj_16 = { a: 0 };
    var obj_17 = { a: 0 };
    var obj_18 = { a: 0 };
    var obj_19 = { a: 0 };
    var obj_20 = { a: 0 };
    var obj_21 = { a: 0 };
    var obj_22 = { a: 0 };
    var obj_23 = { a: 0 };
    var obj_24 = { a: 0 };
    var obj_25 = { a: 0 };
    var obj_26 = { a: 0 };
    var obj_27 = { a: 0 };
    var obj_28 = { a: 0 };
    var obj_29 = { a: 0 };
    var obj_30 = { a: 0 };

    // Doing a bailout after the return of the function call implies that we
    // cannot resume before the function call.  Thus, we would have to recover
    // the 30 objects allocations during the bailout.
    schedulegc(i % 100);
    bailout();

    obj_1.a = 1;
    obj_2.a = 1;
    obj_3.a = 1;
    obj_4.a = 1;
    obj_5.a = 1;
    obj_6.a = 1;
    obj_7.a = 1;
    obj_8.a = 1;
    obj_9.a = 1;
    obj_10.a = 1;
    obj_11.a = 1;
    obj_12.a = 1;
    obj_13.a = 1;
    obj_14.a = 1;
    obj_15.a = 1;
    obj_16.a = 1;
    obj_17.a = 1;
    obj_18.a = 1;
    obj_19.a = 1;
    obj_20.a = 1;
    obj_21.a = 1;
    obj_22.a = 1;
    obj_23.a = 1;
    obj_24.a = 1;
    obj_25.a = 1;
    obj_26.a = 1;
    obj_27.a = 1;
    obj_28.a = 1;
    obj_29.a = 1;
    obj_30.a = 1;
}

for (var i = 0; i < 200; i++) {
    // Do not inline 'g', to keep re-entering 'g' at every loop iteration.
    with (o) { }
    g(i);
}
