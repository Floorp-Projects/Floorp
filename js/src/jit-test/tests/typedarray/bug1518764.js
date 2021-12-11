// |jit-test| error:dead object

var g = newGlobal({newCompartment: true});
var ta = new g.Int32Array(1);
Int32Array.prototype.filter.call(ta, function() {
    nukeAllCCWs();
    return true;
});
