/* global TestFunctions */

postMessage("Done", "*");

var p = new Promise(function () {
  TestFunctions.throwUncatchableException();
  ok(false, "Shouldn't get here!");
}).catch(function () {
  ok(false, "Shouldn't get here!");
});
ok(false, "Shouldn't get here!");
