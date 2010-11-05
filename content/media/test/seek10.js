function test_seek10(v, seekTime, is, ok, finish) {

// Test bug 523335 - ensure that if we close a stream while seeking, we
// don't hang during shutdown. This test won't "fail" per se if it's regressed,
// it will instead start to cause random hangs in the mochitest harness on
// shutdown.

function startTest() {
  // Must be duration*0.9 rather than seekTime, else we don't hit that problem.
  // This is probably due to the seek bisection finishing too quickly, before
  // we can close the stream.
  v.currentTime = v.duration * 0.9;
}

function done(evt) {
  ok(true, "We don't acutally test anything...");
  finish();
}

function seeking() {
  v.onerror = done;
  v.src = "not a valid video file.";
  v.load(); // Cause the existing stream to close.
}

v.addEventListener("loadeddata", startTest, false);
v.addEventListener("seeking", seeking, false);

}
