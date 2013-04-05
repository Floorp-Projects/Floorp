// |jit-test| ion-eager; error: TypeError

function testMonitorIntrinsic() {
  var N = 2;
  // Make an NxN array of zeros.
  var p = new ParallelArray([N,N], function () 0);
  // |i| will go out of bounds!
  for (var i = 0; i < N+1; i++) {
    for (var j = 0; j < 2; j++) {
      // When |i| goes out of bounds, we will bail from Ion to BC on an
      // 'undefined' result inside parallel array's .get, tripping a type
      // barrier on GETINTRINSIC.
      p.get(i).get(j);
    }
  }
}

testMonitorIntrinsic();
