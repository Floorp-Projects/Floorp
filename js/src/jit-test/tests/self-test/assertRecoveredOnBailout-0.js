function f () {
    var o = {};
    var x = assertRecoveredOnBailout(o, true);
    bailout();
    return x;
}

f();
f();
