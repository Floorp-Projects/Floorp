function test_seek7(v, seekTime, is, ok, finish) {

// If a NaN is passed to currentTime, make sure this is caught
// otherwise an infinite loop in the Ogg backend occurs.
var completed = false;
var thrown1 = false;
var thrown2 = false;
var thrown3 = false;

function startTest() {
  if (completed)
    return;

  try {
    v.currentTime = NaN;
  } catch(e) {
    thrown1 = true;
  }

  try {
    v.currentTime = Math.random;
  } catch(e) {
    thrown3 = true;
  }

  completed = true;
  ok(thrown1, "Setting currentTime to invalid value of NaN");
  ok(thrown3, "Setting currentTime to invalid value of a function");
  finish();
}

v.addEventListener("loadedmetadata", startTest, false);

}
