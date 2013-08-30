// |jit-test| slow;
load(libdir + "parallelarray-helpers.js");

// The MathCache has 4096 entries, so ensure we overwrite at least one
// entry by using > 4096 distinct inputs.

var len = 5000;
var iters = 100;

// The problem we are trying to expose (Bugzilla 901000) is
// a data-race on the MathCache; such a bug is inherently
// non-deterministic.  On a 4 core Mac laptop:
//
// len == 10000 iters==1  replicates the problem on 2/10 runs,
// len == 10000 iters==2  replicates the problem on 3/10 runs,
// len == 10000 iters==5  replicates the problem on 6/10 runs,
// len == 10000 iters==10 replicates the problem on 9/10 runs.
//
// len == 5000  iters==1  replicates the problem on 1/10 runs,
// len == 5000  iters==2  replicates the problem on 4/10 runs,
// len == 5000  iters==5  replicates the problem on 5/10 runs,
// len == 5000  iters==10 replicates the problem on 9/10 runs.
//
// len == 2000  iters==1  replicates the problem on 0/10 runs,
// len == 2000  iters==2  replicates the problem on 0/10 runs,
// len == 2000  iters==5  replicates the problem on 0/10 runs,
// len == 2000  iters==10 replicates the problem on 3/10 runs

function check(fill) {
  var seq = Array.build(len, fill);
  for (var i = 0; i < iters; i++) {
    var par = Array.buildPar(len, fill);
    assertStructuralEq(par, seq);
  }
}

function checkAbs(a)   { check(function (i) { return Math.abs(a[i]);   }); }
function checkAcos(a)  { check(function (i) { return Math.acos(a[i]);  }); }
function checkAsin(a)  { check(function (i) { return Math.asin(a[i]);  }); }
function checkAtan(a)  { check(function (i) { return Math.atan(a[i]);  }); }
function checkAtan2(a) { check(function (i) { return Math.atan2(a[i]); }); }
function checkCeil(a)  { check(function (i) { return Math.ceil(a[i]);  }); }
function checkCos(a)   { check(function (i) { return Math.cos(a[i]);   }); }
function checkExp(a)   { check(function (i) { return Math.exp(a[i]);   }); }
function checkFloor(a) { check(function (i) { return Math.floor(a[i]); }); }
function checkLog(a)   { check(function (i) { return Math.log(a[i]);   }); }
function checkRound(a) { check(function (i) { return Math.round(a[i]); }); }
function checkSin(a)   { check(function (i) { return Math.sin(a[i]);   }); }
function checkSqrt(a)  { check(function (i) { return Math.sqrt(a[i]);  }); }
function checkTan(a)   { check(function (i) { return Math.tan(a[i]);   }); }

function callVariousUnaryMathFunctions() {
  // We might want to consider making this test adopt seedrandom.js
  // and call Math.seedrandom("seed") here ...
  // function fill(i) { return (2*Math.random())-1; }
  // function fill(i) { return (20*Math.random())-10; }
  //
  // ... but its easiest to just drop the pseudo-random input entirely.
  function fill(i) { return 10/i; }
  var input = Array.build(len, fill);

  checkAbs(input);    //print("abs");
  checkAcos(input);   //print("acos");
  checkAsin(input);   //print("asin");
  checkAtan(input);   //print("atan");

  checkAtan2(input);  //print("atan2");
  checkCeil(input);   //print("ceil");
  checkCos(input);    //print("cos");
  checkExp(input);    //print("exp");

  checkFloor(input);  //print("floor");
  checkLog(input);    //print("log");
  checkRound(input);  //print("round");
  checkSin(input);    //print("sin");

  checkSqrt(input);   //print("sqrt");
  checkTan(input);    //print("tan");
}

if (getBuildConfiguration().parallelJS)
  callVariousUnaryMathFunctions();
