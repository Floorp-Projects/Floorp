function test_seek2(v, seekTime, is, ok, finish) {

// Test seeking works if current time is set before video is
// playing.
var startPassed = false;
var endPassed = false;
var completed = false;

function startTest() {
  if (completed)
    return;

  v.currentTime=seekTime;
  v.play();
}

function seekStarted() {
  if (completed)
    return;

  ok(v.currentTime >= seekTime - 0.1, "Video currentTime should be around " + seekTime + ": " + v.currentTime);
  startPassed = true;
}

function seekEnded() {
  if (completed)
    return;

  endPassed = true;
}

function playbackEnded() {
  if (completed)
    return;

  completed = true;
  ok(startPassed, "send seeking event");
  ok(endPassed, "send seeked event");
  ok(v.ended, "Checking playback has ended");
  ok(Math.abs(v.currentTime - v.duration) <= 0.1, "Checking currentTime at end: " + v.currentTime);
  finish();
}

v.addEventListener("ended", playbackEnded, false);
v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", seekStarted, false);
v.addEventListener("seeked", seekEnded, false);

}
