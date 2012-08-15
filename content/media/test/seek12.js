function test_seek12(v, seekTime, is, ok, finish) {
var completed = false;

function startTest() {
  if (completed)
    return;
  ok(!v.seeking, "seeking should default to false");
  v.currentTime = seekTime;
  is(v.currentTime, seekTime, "currentTime must report seek target immediately");
  is(v.seeking, true, "seeking flag on start should be true");
}

function seekStarted() {
  if (completed)
    return;
  //is(v.currentTime, seekTime, "seeking: currentTime must be seekTime");
  ok(Math.abs(v.currentTime - seekTime) < 0.01, "seeking: currentTime must be seekTime");
}

function seekEnded() {
  if (completed)
    return;
  completed = true;
  //is(v.currentTime, seekTime, "seeked: currentTime must be seekTime");
  ok(Math.abs(v.currentTime - seekTime) < 0.01, "seeked: currentTime must be seekTime");
  is(v.seeking, false, "seeking flag on end should be false");
  finish();
}

v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeking", seekStarted, false);
v.addEventListener("seeked", seekEnded, false);
}
