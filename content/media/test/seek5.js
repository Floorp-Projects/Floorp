function test_seek5(v, seekTime, is, ok, finish) {

// Test for a seek, followed by a play before the seek completes, ensure we play at the end of the seek.
var startPassed = false;
var endPassed = false;
var completed = false;

function startTest() {
  if (completed)
    return;

  v.currentTime=seekTime;
}

function seekStarted() {
  if (completed)
    return;
  ok(v.currentTime >= seekTime - 0.1, "Video currentTime should be around " + seekTime + ": " + v.currentTime);
  startPassed = true;
  v.play();
}

function seekEnded() {
  if (completed)
    return;
  endPassed = true;
}

function playbackEnded() {
  if (completed)
    return;
  ok(startPassed, "Got seeking event");
  ok(endPassed, "Got seeked event");
  completed = true;
  finish();
}

v.addEventListener("ended", playbackEnded, false);
v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", seekStarted, false);
v.addEventListener("seeked", seekEnded, false);

}
