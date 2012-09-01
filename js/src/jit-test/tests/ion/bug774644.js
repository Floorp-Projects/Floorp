// |jit-test| error: ReferenceError
function YearFromTime() {}
addTestCase();
function addTestCase() {
  var start = -62126352000000;
  YearFromTime();
  var stop = -62094729600000;
  for (var d = start; d < stop; d >>= 86400000)
    new TestCase();
}
