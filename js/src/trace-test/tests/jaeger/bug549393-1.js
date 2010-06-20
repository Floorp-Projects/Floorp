// |trace-test| error: TypeError
function start() {
  MAX_TOTAL_TIME = startTime = new Date
  do {
  if (rnd(0)) return (a[rnd()])()
    lastTime = new Date
  } while ( lastTime - startTime < MAX_TOTAL_TIME )
}
function MersenneTwister19937() {
  this.init_genrand = function() {
    for (mti = 1; mti < 4; mti++) {
      Array[mti] = 1
    }
  };
  this.genrand_int32 = function() {
    if (mti > 4) {
      mti = 0
    }
    return Array[mti++];
  }
} (function() {
  fuzzMT = new MersenneTwister19937;
  fuzzMT.init_genrand()
  rnd = function() {
    return Math.floor(fuzzMT.genrand_int32())
  }
} ())
function weighted(wa) {
  a = []
  for (i = 0; i < wa.length; ++i) {
    for (var j = 0; j < 8; ++j) {
      a.push(wa[i].fun)
    }
  }
}
statementMakers = weighted([{
  fun: function makeMixedTypeArray() { [[, , , , , , , , , , , , , , , , , , ,
, , , , , ""][(a[rnd()])()]]}
}])
start() 

/* Don't assert. */

