function test_fragment_noplay(v, start, end, is, ok, finish) {

function onLoadedMetadata() {
  var s = start == null ? 0 : start;
  var e = end == null ? v.duration : end;
  var a = s - 0.15;
  var b = s + 0.15;
  ok(v.currentTime >= a && v.currentTime <= b, "loadedmetadata currentTime is " + a + " < " + v.currentTime + " < " + b);
  ok(v.mozFragmentEnd == e, "mozFragmentEnd (" + v.mozFragmentEnd + ") == end Time (" + e + ")");
  finish();
}

v.addEventListener("loadedmetadata", onLoadedMetadata, false);
}
