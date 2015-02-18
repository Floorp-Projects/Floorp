// |reftest| skip-if(!xulRuntime.shell) -- uses shell load() function

var BUGNUMBER = 918987;
var summary = 'String.prototype.normalize - part0';

print(BUGNUMBER + ": " + summary);

function test() {
  load('ecma_6/String/normalize-generateddata-input.js');

  for (var test0 of tests_part0) {
    runNormalizeTest(test0);
  }
}

if ("normalize" in String.prototype) {
  // String.prototype.normalize is not enabled in all builds.
  test();
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
