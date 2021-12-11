// Prevent the GC from cancelling Ion compilations, when we expect them to succeed
gczeal(0);

function f () {
    var o = {};
    var x = assertRecoveredOnBailout(o, true);
    bailout();
    return x;
}

f();
f();
