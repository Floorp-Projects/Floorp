function f () {
    var x = new Uint8Array(4);
    empty();
    assertRecoveredOnBailout(x, true);
    var res = inIon();
    bailout();
    return res;
}

function empty() {
}

while(!f());
