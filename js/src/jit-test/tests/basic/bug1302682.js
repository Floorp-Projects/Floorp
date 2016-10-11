for (var i = 0; i < 30000; i++) {
    var a = inIon() ? 7 : 300;
    var buf = new Uint8ClampedArray(a);
    (function() {}) * this;
    try {} catch (e) {}
}
