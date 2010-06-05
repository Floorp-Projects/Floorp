function test_seek11(v, seekTime, is, ok, finish) {

// Test for bug 476973, multiple seeks to the same position shouldn't cause problems.

var seekedNonZero = false;
var completed = false;
var target = 0;

function startTest() {
  if (completed)
    return false;
  target = v.duration / 2;
  v.currentTime = target;
  v.currentTime = target;
  return false;
}

function startSeeking() {
  if (!seekedNonZero) {
    v.currentTime = target;
  }
}

function seekEnded() {
  if (completed)
    return false;

  if (v.currentTime > 0) {
    ok(v.currentTime > target - 0.1 && v.currentTime < target + 0.1,
       "Seek to wrong destination " + v.currentTime);
    seekedNonZero = true;
    v.currentTime = 0.0;
  } else {
    ok(seekedNonZero, "Successfully seeked to nonzero");
    ok(true, "Seek back to zero was successful");
    completed = true;
    finish();
  }

  return false;
}

v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", startSeeking, false);
v.addEventListener("seeked", seekEnded, false);

}
