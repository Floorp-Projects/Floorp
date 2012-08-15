function test_seek11(v, seekTime, is, ok, finish) {

// Test for bug 476973, multiple seeks to the same position shouldn't cause problems.

var seekedNonZero = false;
var completed = false;
var target = 0;

function startTest() {
  if (completed)
    return;
  target = v.duration / 2;
  v.currentTime = target;
  v.currentTime = target;
  v._seekTarget = target;
}

function startSeeking() {
  ok(v.currentTime >= v._seekTarget - 0.1,
     "Video currentTime should be around " + v._seekTarget + ": " + v.currentTime);
  if (!seekedNonZero) {
    v.currentTime = target;
    v._seekTarget = target;
    seekedNonZero = true;
  }
}

function seekEnded() {
  if (completed)
    return;

  if (v.currentTime > 0) {
    ok(v.currentTime > target - 0.1 && v.currentTime < target + 0.1,
       "Seek to wrong destination " + v.currentTime);
    v.currentTime = 0.0;
    v._seekTarget = 0.0;
  } else {
    ok(seekedNonZero, "Successfully seeked to nonzero");
    ok(true, "Seek back to zero was successful");
    completed = true;
    finish();
  }
}

v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", startSeeking, false);
v.addEventListener("seeked", seekEnded, false);

}
