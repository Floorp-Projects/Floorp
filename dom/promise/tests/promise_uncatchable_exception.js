postMessage("Done", "*");

var p = new Promise(function(resolve, reject) {
  TestFunctions.throwUncatchableException();
  ok(false, "Shouldn't get here!");
}).catch(function(exception) {
  ok(false, "Shouldn't get here!");
});
ok(false, "Shouldn't get here!");
