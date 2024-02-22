function testMathyFunction (f, inputs) {
  var results = [];
    for (var j = 0; j < inputs.length; ++j)
      for (var k = 0; k < inputs.length; ++k)
        results.push(f(inputs[j], inputs[k]));
}
mathy1=(function(stdlib,foreign,heap){
  ff = foreign.ff;
  Float32ArrayView = new stdlib.Float32Array(heap);
  Uint32ArrayView = new stdlib.Uint32Array(heap);
  function f(d0) {
    var i2=0;
    var i4;
    i2=Float32ArrayView[2];
    i4=i2;
    ff(2,0) ? f : 6;
    Uint32ArrayView[!!d0] + [...[eval]]
    return i4 ? 1 : 0;
  }
return f
})(this,{ ff:(Function('')) },new SharedArrayBuffer(40));
testMathyFunction(mathy1,[Math.N,Number.M,(2),Number.M])
