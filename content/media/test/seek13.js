function test_seek13(v, seekTime, is, ok, finish) {
var completed = false;

function startTest() {
  if (completed)
    return false;
  ok(!v.seeking, "seeking should default to false");
  v.currentTime = v.duration;
  is(v.currentTime, v.duration, "currentTime must report seek target immediately");
  is(v.seeking, true, "seeking flag on start should be true");
  return false;
}

function seekStarted() {
  if (completed)
    return false;
  //is(v.currentTime, v.duration, "seeking: currentTime must be duration");
  ok(Math.abs(v.currentTime - v.duration) < 0.01, "seeking: currentTime must be duration");
  return false;
}

function seekEnded() {
  if (completed)
    return false;
  //is(v.currentTime, v.duration, "seeked: currentTime must be duration");
  ok(Math.abs(v.currentTime - v.duration) < 0.01, "seeked: currentTime must be duration");
  is(v.seeking, false, "seeking flag on end should be false");
  return false;
}

function playbackEnded() {
  if (completed)
    return false;
  completed = true;
  //is(v.currentTime, v.duration, "ended: currentTime must be duration");
  ok(Math.abs(v.currentTime - v.duration) < 0.01, "ended: currentTime must be duration");
  is(v.seeking, false, "seeking flag on end should be false");
  is(v.ended, true, "ended must be true");
  finish();
  return false;
}

v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", seekStarted, false);
v.addEventListener("seeked", seekEnded, false);
v.addEventListener("ended", playbackEnded, false);
}
