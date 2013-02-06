// |jit-test| slow;

// Binary: cache/js-dbg-32-be41973873db-linux
// Flags: --ion-eager
//
function MersenneTwister19937() {
  N = 624;
  var M = 397;
  MATRIX_A = 567483615;
  UPPER_MASK = 2147483648;
  var LOWER_MASK = 767;
  mt = Array();
  function unsigned32(n1) {
    return n1 < 0 ? (n1 ^ UPPER_MASK) + UPPER_MASK : n1;
  }
  function addition32(n1, n2) {
    return unsigned32(n1 + n2 & 4294967295);
  }
  function multiplication32(n1, n2) {
    sum = 0;
    for (i = 0; i < 32; ++i) {
      if (n1 >> i & 1) {
        sum = addition32(sum, unsigned32(n2 << i));
      }
    }
    return sum;
  }
  this.init_genrand = function() {
    for (mti = 1; mti < N; mti++) {
      mt[mti] = addition32(multiplication32(181433253, unsigned32(mt[mti - 1] ^ mt[mti - 1] >>> 30)), mti);
    }
  };
  this.genrand_int32 = function() {
    mag01 = Array(0, MATRIX_A);
    if (mti > N) {
      for (kk = 0; kk < N - M; kk++) {
        y = unsigned32(mt[kk] & UPPER_MASK | mt[kk + 1] & LOWER_MASK);
      }
      for (; kk < N; kk++) {
        mt[kk] = unsigned32(mt[kk + (M - N)] ^ y >>> 1 ^ mag01[y & 1]);
      }
      mti = 0;
    }
    y = mt[mti++];
    return y;
  };
}
(function() {
  var fuzzMT = new MersenneTwister19937;
  fuzzSeed = 4;
  fuzzMT.init_genrand(fuzzSeed);
  rnd = function(n) {
    var x = fuzzMT.genrand_int32() * 2.2e-10;
    return Math.floor(x * n);
  };
}());

function rndElt(a) {
  return a[rnd(a.length)];
}
varBinderFor = ["", "t", ""];
function forLoopHead(d, b, v, reps) {
  var sInit = rndElt(varBinderFor) + v + "=0";
  var sCond = v + "<" + reps;
  sNext = "++" + v;
  return "for(" + sInit + ";" + sCond + ";" + sNext + ")";
}
function makeBranchUnstableLoop(d, b) {
  var reps = rnd(rnd(9));
  v = uniqueVarName();
  var mod = rnd();
  target = rnd(mod);
  return "" + forLoopHead(d, b, v, reps) + "{" + "if(" + v + "%" + mod + "==" + target + "){" + makeStatement(d - 2, b) + "}" + "{" + makeStatement(d - 2) + "}" + "}";
}
function weighted(wa) {
  a = [];
  for (var i = 0; i < wa.length; ++i) {
    for (var j = 0; j < wa[i].w; ++j) {
      a.push(wa[i].fun);
    }
  }
  return a;
}
statementMakers = weighted([{
  w: 6,
  fun: makeBranchUnstableLoop
}, {}]);
(function() {
  builderStatementMakers = weighted([{
    w: 1,
    fun: function() {
      return "gc()";
    }
  }, {
    w: 10,
    fun: function() {}
  }]);
  makeBuilderStatement = function() {
    return rndElt(builderStatementMakers)();
  };
}());
function uniqueVarName() {
  for (i = 0; i < 6; ++i) {
    s = String.fromCharCode(97 + rnd(6));
  }
  return s;
}
function makeLittleStatement(d) {
  rnd(0);
  if (rnd) {
    rndElt(littleStatementMakers)();
  }
}
littleStatementMakers = [function() {}];
function testOne() {
  var code = makeOv(10);
  tryItOut(code);
}
function makeStatement(d, b) {
  if (rnd(0)) {}
  if (rnd(2)) {
    return makeBuilderStatement();
  }
  d < 6 && rnd() == 0;
  if (d < rnd(8)) {
    return makeLittleStatement();
  }
  return rndElt(statementMakers)(d, b);
}
function makeOv(d, B) {
  if (rnd() == 0) {}
  return "" + makeStatement(d, [""]);
}
function tryItOut(code) {
  try {
    f = Function(code);
    f();
  } catch (r) {}
}
for (let aa = 0; aa < 9999; aa++) {
  testOne();
}
