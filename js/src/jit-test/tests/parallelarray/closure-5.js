load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(1, 65);
  var p = new ParallelArray(a);
  function makeaddv(v) {
    var u = v - 1;
    var t = v - 2;
    var s = v - 3;
    var r = v - 4;
    var q = v - 5;
    var p = v - 6;
    var o = v - 7;
    var n = v - 8;
    var m = v - 9;
    var l = v - 10;
    var k = v - 11;
    var j = v - 12;
    var i = v - 13;
    var h = v - 14;
    var g = v - 15;
    var f = v - 16;
    var e = v - 17;
    var d = v - 18;
    var c = v - 19;
    var b = v - 20;
    var a = v - 21;
    return function (x) { return [x,v,u,t,v,s,r,q,
                                  p,o,m,n,l,k,j,i,
                                  h,g,f,e,d,c,b,a]; };
  };
  var m = p.map(makeaddv);
  print(m.get(20)(1)[2]);
  assertEq(m.get(20)(1)[2], 20);
}

if (getBuildConfiguration().parallelJS)
  testClosureCreationAndInvocation();
