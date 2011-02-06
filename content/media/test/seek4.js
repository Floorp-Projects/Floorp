function test_seek4(v, seekTime, is, ok, finish) {

// Test for a seek, followed by another seek before the first is complete.
var seekCount = 0;
var completed = false;

function startTest() {
  if (completed)
    return false;

  v.currentTime=seekTime;
  v._seekTarget=seekTime;
  return false;
}

function seekStarted() {
  if (completed)
    return false;

  seekCount += 1;

  ok(v.currentTime >= v._seekTarget - 0.1,
     "Video currentTime should be around " + v._seekTarget + ": " + v.currentTime);
  if (seekCount == 1) {
    v.currentTime=seekTime/2;
    v._seekTarget=seekTime/2;
  }
  return false;
}

function seekEnded() {
  if (completed)
    return false;

  if (seekCount == 2) {
    ok(Math.abs(v.currentTime - seekTime/2) <= 0.1, "seek on target: " + v.currentTime);
    completed = true;
    finish();
  }

  return false;
}

v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", seekStarted, false);
v.addEventListener("seeked", seekEnded, false);

}
