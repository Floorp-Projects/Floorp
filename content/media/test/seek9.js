function test_seek9(v, seekTime, is, ok, finish) {

var completed = false;

function startTest() {
  v.currentTime = -1000;
}

function seekEnded() {
  if (completed)
    return false;

  is(v.currentTime, 0, "currentTime clamped to 0");
  finish();
}

v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeked", seekEnded, false);

}
