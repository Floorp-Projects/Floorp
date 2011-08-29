function test_fragment_noplay(v, start, end, is, ok, finish) {

var completed = false;

function onLoadedMetadata() {
  var s = start == null ? 0 : start;
  var e = end == null ? v.duration : end;
  var a = s - 0.15;
  var b = s + 0.15;
  ok(v.currentTime >= a && v.currentTime <= b, "loadedmetadata currentTime is " + a + " < " + v.currentTime + " < " + b);
  ok(v.initialTime == s, "initialTime (" + v.initialTime + ") == start Time (" + s + ")");
  ok(v.mozFragmentEnd == e, "mozFragmentEnd (" + v.mozFragmentEnd + ") == end Time (" + e + ")");
  completed = true;
  finish();
  return false;
}

v.addEventListener("loadedmetadata", onLoadedMetadata, false);
}
