function test_seek9(v, seekTime, is, ok, finish) {

function startTest() {
  v.currentTime = -1000;
}

function seekEnded() {
  is(v.currentTime, 0, "currentTime clamped to 0");
  finish();
}

v.addEventListener("loadedmetadata", startTest, false);
v.addEventListener("seeked", seekEnded, false);

}
