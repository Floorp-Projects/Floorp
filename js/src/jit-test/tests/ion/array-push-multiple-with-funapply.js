// |jit-test| --no-threads

// This test case check's Ion ability to inline Array.prototype.push, when
// fun.apply is used and inlined with the set of arguments of the current
// function. Note, that the following are not equivalent in case of failures:
//
//   arr = [];
//   arr.push(1,2,3); // OOM ---> arr == []
//
//   arr = [];
//   arr.push(1);
//   arr.push(2); // OOM --> arr == [1]
//   arr.push(3);

function canIoncompile() {
  while (true) {
    var i = inIon();
    if (i)
      return i;
  }
}

if (canIoncompile() != true)
  quit();
if ("gczeal" in this)
  gczeal(0);

function pushLimits(limit, offset) {
  function pusher() {
    Array.prototype.push.apply(arr, arguments)
  }
  var arr = [0,1,2,3,4,5,6,7];
  arr.length = offset;
  var l = arr.length;
  var was = inIon();
  oomAtAllocation(limit);
  try {
    for (var i = 0; i < 100; i++)
      pusher(0,1,2,3,4,5,6,7);
  } catch (e) {
    // Catch OOM.
  }
  resetOOMFailure();
  assertEq(arr.length % 8, l);
  // Check for a bailout.
  var is = inIon();
  return was ? is ? 1 : 2 : 0;
}



// We need this limit to be high enough to be able to OSR in the for-loop of
// pushLimits.
var limit = 1024 * 1024 * 1024;
while(true) {
  var res = pushLimits(limit, 0);
  print(limit, res);

  if (res == 0) {
    limit = 1024 * 1024 * 1024;
  } else if (res == 1) { // Started and finished in Ion.
    // We want to converge quickly to a state where the memory is limited
    // enough to cause failures in array.prototype.push.
    limit = (limit / 1.5) | 0;
    if (limit == 0) // If we are not in the Jit.
      break;
  } else if (res == 2) { // Started in Ion, and finished in Baseline.
    if (limit < 10) {
      // This is used to offset the OOM location, such that we can test
      // each steps of the Array.push function, when it is jitted.
      for (var off = 1; off < 8; off++)
        pushLimits(limit, off);
    }
    if (limit == 1)
      break;
    limit--;
  }
}
