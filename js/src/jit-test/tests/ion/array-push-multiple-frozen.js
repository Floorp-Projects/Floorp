// |jit-test| --no-threads; skip-if: !('oomAtAllocation' in this)

// This test case check's Ion ability to recover from an allocation failure in
// the inlining of Array.prototype.push, when given multiple arguments. Note,
// that the following are not equivalent in case of failures:
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

function pushLimits(limit, offset, arr, freeze) {
  arr = arr || [];
  arr.push(0,1,2,3,4,5,6,7,8,9);
  arr.length = offset;
  var l = arr.length;
  var was = inIon();
  oomAtAllocation(limit);
  try {
    for (var i = 0; i < 50; i++)
      arr.push(0,1,2,3,4,5,6,7,8,9);
    if (freeze)
      arr.frozen();
    for (var i = 0; i < 100; i++)
      arr.push(0,1,2,3,4,5,6,7,8,9);
  } catch (e) {
    // Catch OOM.
  }
  resetOOMFailure();
  assertEq(arr.length % 10, l);
  // Check for a bailout.
  var is = inIon();
  return was ? is ? 1 : 2 : 0;
}

// We need this limit to be high enough to be able to OSR in the for-loop of
// pushLimits.
var limit = 1024 * 1024 * 1024;
while(true) {
  var res = pushLimits(limit, 0);

  if (res == 0) {
    limit = 1024 * 1024 * 1024;
  } else if (res == 1) { // Started and finished in Ion.
    if (limit <= 1) // If we are not in the Jit.
      break;
    // We want to converge quickly to a state where the memory is limited
    // enough to cause failures in array.prototype.push.
    limit = (limit / 2) | 0;
  } else if (res == 2) { // Started in Ion, and finished in Baseline.
    if (limit < 10) {
      // This is used to offset the OOM location, such that we can test
      // each steps of the Array.push function, when it is jitted.
      for (var off = 1; off < 10; off++) {
        var arr = [];
        try {
          pushLimits(limit, off, arr, true);
        } catch (e) {
          // Cacth OOM produced while generating the error message.
        }
        if (arr.length > 10) assertEq(arr[arr.length - 1], 9);
        else assertEq(arr[arr.length - 1], arr.length - 1);
      }
    }
    if (limit == 1)
      break;
    limit--;
  }
}
