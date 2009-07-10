function test_seek5(v, seekTime, is, ok, finish) {

// Test for a seek, followed by a play before the seek completes, ensure we play at the end of the seek.
var startPassed = false;
var endPassed = false;
var completed = false;

function startTest() {
  if (completed)
    return false;

  v.currentTime=seekTime;
  return false;
}

function seekStarted() {
  if (completed)
    return false;
  startPassed = true;
  v.play();
  return false;
}

function seekEnded() {
  if (completed)
    return false;
  endPassed = true;
  return false;
}

function playbackEnded() {
  if (completed)
    return false;
  ok(startPassed, "Got seeking event");
  ok(endPassed, "Got seeked event");
  completed = true;
  finish();
  return false;
}

v.addEventListener("ended", playbackEnded, false);
v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", seekStarted, false);
v.addEventListener("seeked", seekEnded, false);

}
