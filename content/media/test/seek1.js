function test_seek1(v, seekTime, is, ok, finish) {

var startPassed = false;
var endPassed = false;
var seekFlagStart = false;
var seekFlagEnd = false;
var readonly = true;
var completed = false;

function startTest() {
  if (completed)
    return false;
  ok(!v.seeking, "seeking should default to false");
  try {
    v.seeking = true;
    readonly = v.seeking === false;
  }
  catch(e) {
    readonly = "threw exception: " + e;
  }
  is(readonly, true, "seeking should be readonly");

  v.play();
  v.currentTime=seekTime;
  seekFlagStart = v.seeking;
  return false;
}

function seekStarted() {
  if (completed)
    return false;
  ok(v.currentTime >= seekTime - 0.1,
     "Video currentTime should be around " + seekTime + ": " + v.currentTime + " (seeking)");
  v.pause();
  startPassed = true;
  return false;
}

function seekEnded() {
  if (completed)
    return false;

  var t = v.currentTime;
  // Since we were playing, and we only paused asynchronously, we can't be
  // sure that we paused before the seek finished, so we may have played
  // ahead arbitrarily far.
  ok(t >= seekTime - 0.1, "Video currentTime should be around " + seekTime + ": " + t + " (seeked)");
  v.play();
  endPassed = true;
  seekFlagEnd = v.seeking;
  return false;
}

function playbackEnded() {
  if (completed)
    return false;

  completed = true;
  ok(startPassed, "seeking event");
  ok(endPassed, "seeked event");
  ok(seekFlagStart, "seeking flag on start should be true");
  ok(!seekFlagEnd, "seeking flag on end should be false");
  finish();
  return false;
}

v.addEventListener("ended", playbackEnded, false);
v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", seekStarted, false);
v.addEventListener("seeked", seekEnded, false);

}
